#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <io.h>
#include <fcntl.h>
#include <llarp/util/thread/queue.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/router/abstractrouter.hpp>

#include <fmt/std.h>

// DDK macros
#define CTL_CODE(DeviceType, Function, Method, Access) \
  (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define FILE_DEVICE_UNKNOWN 0x00000022
#define FILE_ANY_ACCESS 0x00000000
#define METHOD_BUFFERED 0

/* From OpenVPN tap driver, common.h */
#define TAP_CONTROL_CODE(request, method) \
  CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
#define TAP_IOCTL_GET_MAC TAP_CONTROL_CODE(1, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION TAP_CONTROL_CODE(2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU TAP_CONTROL_CODE(3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO TAP_CONTROL_CODE(4, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE(5, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS TAP_CONTROL_CODE(6, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ TAP_CONTROL_CODE(7, METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE TAP_CONTROL_CODE(8, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT TAP_CONTROL_CODE(9, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_TUN TAP_CONTROL_CODE(10, METHOD_BUFFERED)

/* Windows registry crap */
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define NETWORK_ADAPTERS                                                 \
  "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-" \
  "08002BE10318}"

typedef unsigned long IPADDR;

namespace llarp::vpn
{
  static char*
  reg_query(char* key_name)
  {
    HKEY adapters, adapter;
    DWORD i, ret, len;
    char* deviceid = nullptr;
    DWORD sub_keys = 0;

    ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(key_name), 0, KEY_READ, &adapters);
    if (ret != ERROR_SUCCESS)
    {
      return nullptr;
    }

    ret = RegQueryInfoKey(
        adapters, NULL, NULL, NULL, &sub_keys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (ret != ERROR_SUCCESS)
    {
      return nullptr;
    }

    if (sub_keys <= 0)
    {
      return nullptr;
    }

    /* Walk througt all adapters */
    for (i = 0; i < sub_keys; i++)
    {
      char new_key[MAX_KEY_LENGTH];
      char data[256];
      TCHAR key[MAX_KEY_LENGTH];
      DWORD keylen = MAX_KEY_LENGTH;

      /* Get the adapter key name */
      ret = RegEnumKeyEx(adapters, i, key, &keylen, NULL, NULL, NULL, NULL);
      if (ret != ERROR_SUCCESS)
      {
        continue;
      }

      /* Append it to NETWORK_ADAPTERS and open it */
      snprintf(new_key, sizeof new_key, "%s\\%s", key_name, key);
      ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(new_key), 0, KEY_READ, &adapter);
      if (ret != ERROR_SUCCESS)
      {
        continue;
      }

      /* Check its values */
      len = sizeof data;
      ret = RegQueryValueEx(adapter, "ComponentId", NULL, NULL, (LPBYTE)data, &len);
      if (ret != ERROR_SUCCESS)
      {
        /* This value doesn't exist in this adaptater tree */
        goto clean;
      }
      /* If its a tap adapter, its all good */
      if (strncmp(data, "tap0901", 7) == 0)
      {
        DWORD type;

        len = sizeof data;
        ret = RegQueryValueEx(adapter, "NetCfgInstanceId", NULL, &type, (LPBYTE)data, &len);
        if (ret != ERROR_SUCCESS)
        {
          goto clean;
        }
        deviceid = strdup(data);
        break;
      }
    clean:
      RegCloseKey(adapter);
    }
    RegCloseKey(adapters);
    return deviceid;
  }

  template <typename Visit>
  void
  ForEachWIN32Interface(Visit visit)
  {
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
    MIB_IPFORWARDTABLE* pIpForwardTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(sizeof(MIB_IPFORWARDTABLE));
    if (pIpForwardTable == nullptr)
      return;

    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
    {
      FREE(pIpForwardTable);
      pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(dwSize);
      if (pIpForwardTable == nullptr)
      {
        return;
      }
    }

    if ((dwRetVal = GetIpForwardTable(pIpForwardTable, &dwSize, 0)) == NO_ERROR)
    {
      for (int i = 0; i < (int)pIpForwardTable->dwNumEntries; i++)
      {
        visit(&pIpForwardTable->table[i]);
      }
    }
    FREE(pIpForwardTable);
#undef MALLOC
#undef FREE
  }

  std::optional<int>
  GetInterfaceIndex(huint32_t ip)
  {
    std::optional<int> ret = std::nullopt;
    ForEachWIN32Interface([&ret, n = ToNet(ip)](auto* iface) {
      if (ret.has_value())
        return;
      if (iface->dwForwardNextHop == n.n)
      {
        ret = iface->dwForwardIfIndex;
      }
    });
    return ret;
  }

  namespace
  {
    std::wstring
    get_win_sys_path()
    {
      wchar_t win_sys_path[MAX_PATH] = {0};
      const wchar_t* default_sys_path = L"C:\\Windows\\system32";

      if (!GetSystemDirectoryW(win_sys_path, _countof(win_sys_path)))
      {
        wcsncpy(win_sys_path, default_sys_path, _countof(win_sys_path));
        win_sys_path[_countof(win_sys_path) - 1] = L'\0';
      }
      return win_sys_path;
    }
  }  // namespace

  class Win32Interface final : public NetworkInterface
  {
    std::atomic<bool> m_Run;
    HANDLE m_Device, m_IOCP;
    std::vector<std::thread> m_Threads;
    thread::Queue<net::IPPacket> m_ReadQueue;

    InterfaceInfo m_Info;

    AbstractRouter* const _router;

    static std::string
    NetSHCommand()
    {
      std::wstring wcmd = get_win_sys_path() + L"\\netsh.exe";

      using convert_type = std::codecvt_utf8<wchar_t>;
      std::wstring_convert<convert_type, wchar_t> converter;
      return converter.to_bytes(wcmd);
    }

    static void
    NetSH(std::string commands)
    {
      commands = NetSHCommand() + " interface IPv6 " + commands;
      LogInfo("exec: ", commands);
      ::system(commands.c_str());
    }

   public:
    static std::string
    RouteCommand()
    {
      std::wstring wcmd = get_win_sys_path() + L"\\route.exe";

      using convert_type = std::codecvt_utf8<wchar_t>;
      std::wstring_convert<convert_type, wchar_t> converter;
      return converter.to_bytes(wcmd);
    }

    Win32Interface(InterfaceInfo info, AbstractRouter* router)
        : m_ReadQueue{1024}, m_Info{std::move(info)}, _router{router}
    {
      DWORD len;

      const auto device_id = reg_query(NETWORK_ADAPTERS);
      if (device_id == nullptr)
      {
        LogError("cannot query registry");
        throw std::invalid_argument{"cannot query registery"};
      }
      const auto fname = fmt::format(R"(\\.\Global\{}.tap)", device_id);
      m_Device = CreateFile(
          fname.c_str(),
          GENERIC_WRITE | GENERIC_READ,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          0,
          OPEN_EXISTING,
          FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
          0);
      if (m_Device == INVALID_HANDLE_VALUE)
      {
        LogError("failed to open device");
        throw std::invalid_argument{"cannot open " + fname};
      }

      LogInfo("putting interface up...");
      ULONG flag = 1;
      // put the interface up
      if (not DeviceIoControl(
              m_Device,
              TAP_IOCTL_SET_MEDIA_STATUS,
              &flag,
              sizeof(flag),
              &flag,
              sizeof(flag),
              &len,
              nullptr))
      {
        LogError("cannot up interface up");
        throw std::invalid_argument{"cannot put interface up"};
      }

      LogInfo("setting addresses");
      huint32_t ip{};
      // set ipv4 addresses
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          IPADDR sock[3]{};
          const nuint32_t addr = xhtonl(net::TruncateV6(ifaddr.range.addr));
          ip = net::TruncateV6(ifaddr.range.addr);
          m_Info.ifname = ip.ToString();
          const nuint32_t mask = xhtonl(net::TruncateV6(ifaddr.range.netmask_bits));
          LogInfo("address ", addr, " netmask ", mask);
          sock[0] = addr.n;
          sock[1] = addr.n & mask.n;
          sock[2] = mask.n;

          if (not DeviceIoControl(
                  m_Device,
                  TAP_IOCTL_CONFIG_TUN,
                  &sock,
                  sizeof(sock),
                  &sock,
                  sizeof(sock),
                  &len,
                  nullptr))
          {
            LogError("cannot set address");
            throw std::invalid_argument{"cannot configure tun interface address"};
          }

          IPADDR ep[4]{};

          ep[0] = addr.n;
          ep[1] = mask.n;
          ep[2] = (addr.n | ~mask.n) - htonl(1);
          ep[3] = 31536000;

          if (not DeviceIoControl(
                  m_Device,
                  TAP_IOCTL_CONFIG_DHCP_MASQ,
                  ep,
                  sizeof(ep),
                  ep,
                  sizeof(ep),
                  &len,
                  nullptr))
          {
            LogError("cannot set dhcp masq");
            throw std::invalid_argument{"Cannot configure tun interface dhcp"};
          }
#pragma pack(push)
#pragma pack(1)
          struct opt
          {
            uint8_t dhcp_opt;
            uint8_t length;
            uint32_t value;
          } dns, gateway;
#pragma pack(pop)

          const nuint32_t dnsaddr{xhtonl(m_Info.dnsaddr)};
          dns.dhcp_opt = 6;
          dns.length = 4;
          dns.value = dnsaddr.n;

          gateway.dhcp_opt = 3;
          gateway.length = 4;
          gateway.value = addr.n + htonl(1);

          if (not DeviceIoControl(
                  m_Device,
                  TAP_IOCTL_CONFIG_DHCP_SET_OPT,
                  &gateway,
                  sizeof(gateway),
                  &gateway,
                  sizeof(gateway),
                  &len,
                  nullptr))
          {
            LogError("cannot set gateway");
            throw std::invalid_argument{"cannot set tun gateway"};
          }

          if (not DeviceIoControl(
                  m_Device,
                  TAP_IOCTL_CONFIG_DHCP_SET_OPT,
                  &dns,
                  sizeof(dns),
                  &dns,
                  sizeof(dns),
                  &len,
                  nullptr))
          {
            LogError("cannot set dns");
            throw std::invalid_argument{"cannot set tun dns"};
          }
        }
      }
      // set ipv6 addresses
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET6)
        {
          const auto maybe = GetInterfaceIndex(ip);
          if (maybe.has_value())
          {
            NetSH(
                "add address interface=" + std::to_string(*maybe) + " " + ifaddr.range.ToString());
          }
        }
      }
    }

    ~Win32Interface()
    {
      ULONG flag = 0;
      DWORD len;
      // put the interface down
      DeviceIoControl(
          m_Device,
          TAP_IOCTL_SET_MEDIA_STATUS,
          &flag,
          sizeof(flag),
          &flag,
          sizeof(flag),
          &len,
          nullptr);
      m_Run = false;
      CloseHandle(m_IOCP);
      // close the handle
      CloseHandle(m_Device);
      // close the reader threads
      for (auto& thread : m_Threads)
        thread.join();
    }

    virtual void
    MaybeWakeUpperLayers() const override
    {
      _router->TriggerPump();
    }

    int
    PollFD() const override
    {
      return -1;
    }

    std::string
    IfName() const override
    {
      return m_Info.ifname;
    }

    void
    Start()
    {
      m_Run = true;
      const auto numThreads = std::thread::hardware_concurrency();
      // allocate packets
      for (size_t idx = 0; idx < numThreads; ++idx)
        m_Packets.emplace_back(new asio_evt_pkt{true});

      // create completion port
      m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
      // attach the handle or some shit
      CreateIoCompletionPort(m_Device, m_IOCP, 0, 0);
      // spawn reader threads
      for (size_t idx = 0; idx < numThreads; ++idx)
        m_Threads.emplace_back([this, idx]() { ReadLoop(idx); });
    }

    net::IPPacket
    ReadNextPacket() override
    {
      if (m_ReadQueue.empty())
        return net::IPPacket{};

      return m_ReadQueue.popFront();
    }

    struct asio_evt_pkt
    {
      explicit asio_evt_pkt(bool _read) : read{_read}
      {}

      OVERLAPPED hdr = {0, 0, 0, 0, nullptr};  // must be first, since this is part of the IO call
      bool read;
      net::IPPacket pkt;

      void
      Read(HANDLE dev)
      {
        ReadFile(dev, pkt.buf, sizeof(pkt.buf), nullptr, &hdr);
      }
    };

    std::vector<std::unique_ptr<asio_evt_pkt>> m_Packets;

    bool
    WritePacket(net::IPPacket pkt)
    {
      LogDebug("write packet ", pkt.sz);
      asio_evt_pkt* ev = new asio_evt_pkt{false};
      std::copy_n(pkt.buf, pkt.sz, ev->pkt.buf);
      ev->pkt.sz = pkt.sz;
      WriteFile(m_Device, ev->pkt.buf, ev->pkt.sz, nullptr, &ev->hdr);
      return true;
    }

    void
    ReadLoop(size_t packetIndex)
    {
      auto& ev = m_Packets[packetIndex];
      ev->Read(m_Device);
      while (m_Run)
      {
        DWORD size;
        ULONG_PTR user;
        OVERLAPPED* ovl = nullptr;
        if (not GetQueuedCompletionStatus(m_IOCP, &size, &user, &ovl, 1000))
          continue;
        asio_evt_pkt* pkt = (asio_evt_pkt*)ovl;
        LogDebug("got iocp event size=", size, " read=", pkt->read);
        if (pkt->read)
        {
          pkt->pkt.sz = size;
          m_ReadQueue.pushBack(pkt->pkt);
          pkt->Read(m_Device);
        }
        else
          delete pkt;
      }
    }
  };

  class Win32RouteManager : public IRouteManager
  {
    void
    Execute(std::string cmd) const
    {
      llarp::LogInfo("exec: ", cmd);
      ::system(cmd.c_str());
    }

    static std::string
    PowerShell()
    {
      std::wstring wcmd =
          get_win_sys_path() + L"\\WindowsPowerShell\\v1.0\\powershell.exe -Command ";

      using convert_type = std::codecvt_utf8<wchar_t>;
      std::wstring_convert<convert_type, wchar_t> converter;
      return converter.to_bytes(wcmd);
    }

    static std::string
    RouteCommand()
    {
      return Win32Interface::RouteCommand();
    }

    void
    Route(IPVariant_t ip, IPVariant_t gateway, std::string cmd)
    {
      Execute(fmt::format(
          "{} {} {} MASK 255.255.255.255 {} METRIC 2", RouteCommand(), cmd, ip, gateway));
    }

    void
    DefaultRouteViaInterface(std::string ifname, std::string cmd)
    {
      // poke hole for loopback bacause god is dead on windows
      Execute(RouteCommand() + " " + cmd + " 127.0.0.0 MASK 255.0.0.0 0.0.0.0");

      huint32_t ip{};
      ip.FromString(ifname);
      const auto ipv6 = net::ExpandV4Lan(ip);

      Execute(RouteCommand() + " " + cmd + " ::/2 " + ipv6.ToString());
      Execute(RouteCommand() + " " + cmd + " 4000::/2 " + ipv6.ToString());
      Execute(RouteCommand() + " " + cmd + " 8000::/2 " + ipv6.ToString());
      Execute(RouteCommand() + " " + cmd + " c000::/2 " + ipv6.ToString());

      ifname.back()++;
      Execute(RouteCommand() + " " + cmd + " 0.0.0.0 MASK 128.0.0.0 " + ifname + " METRIC 2");
      Execute(RouteCommand() + " " + cmd + " 128.0.0.0 MASK 128.0.0.0 " + ifname + " METRIC 2");
    }

    void
    RouteViaInterface(std::string ifname, IPRange range, std::string cmd)
    {
      if (range.IsV4())
      {
        const huint32_t addr4 = net::TruncateV6(range.addr);
        const huint32_t mask4 = net::TruncateV6(range.netmask_bits);
        Execute(
            RouteCommand() + " " + cmd + " " + addr4.ToString() + " MASK " + mask4.ToString() + " "
            + ifname);
      }
      else
      {
        Execute(
            RouteCommand() + " " + cmd + range.addr.ToString() + " MASK "
            + range.netmask_bits.ToString() + " " + ifname);
      }
    }

   public:
    void
    AddRoute(IPVariant_t ip, IPVariant_t gateway) override
    {
      Route(ip, gateway, "ADD");
    }

    void
    DelRoute(IPVariant_t ip, IPVariant_t gateway) override
    {
      Route(ip, gateway, "DELETE");
    }

    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(vpn.IfName(), range, "ADD");
    }

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(vpn.IfName(), range, "DELETE");
    }

    std::vector<IPVariant_t>
    GetGatewaysNotOnInterface(std::string ifname) override
    {
      std::vector<IPVariant_t> gateways;
      ForEachWIN32Interface([&](auto w32interface) {
        struct in_addr gateway, interface_addr;
        gateway.S_un.S_addr = (u_long)w32interface->dwForwardDest;
        interface_addr.S_un.S_addr = (u_long)w32interface->dwForwardNextHop;
        std::string interface_name{inet_ntoa(interface_addr)};
        if ((!gateway.S_un.S_addr) and interface_name != ifname)
        {
          llarp::LogTrace(
              "Win32 find gateway: Adding gateway (", interface_name, ") to list of gateways.");
          huint32_t x{};
          if (x.FromString(interface_name))
            gateways.push_back(x);
        }
      });
      return gateways;
    }

    void
    AddDefaultRouteViaInterface(std::string ifname) override
    {
      // kill ipv6
      Execute(PowerShell() + R"(Disable-NetAdapterBinding -Name "*" -ComponentID ms_tcpip6)");
      DefaultRouteViaInterface(ifname, "ADD");
    }

    void
    DelDefaultRouteViaInterface(std::string ifname) override
    {
      // restore ipv6
      Execute(PowerShell() + R"(Enable-NetAdapterBinding -Name "*" -ComponentID ms_tcpip6)");
      DefaultRouteViaInterface(ifname, "DELETE");
    }
  };

  class Win32Platform : public Platform
  {
    Win32RouteManager _routeManager{};

   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override
    {
      auto netif = std::make_shared<Win32Interface>(std::move(info), router);
      netif->Start();
      return netif;
    };

    IRouteManager&
    RouteManager() override
    {
      return _routeManager;
    }
  };

}  // namespace llarp::vpn

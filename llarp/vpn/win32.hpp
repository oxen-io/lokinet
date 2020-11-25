#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <io.h>
#include <fcntl.h>
#include <util/thread/queue.hpp>
#include <ev/vpn.hpp>

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

  class Win32Interface : public NetworkInterface
  {
    std::atomic<bool> m_Run;
    HANDLE m_Device, m_IOCP, m_ReadThread;
    thread::Queue<net::IPPacket> m_ReadQueue;

    const InterfaceInfo m_Info;

   public:
    Win32Interface(InterfaceInfo info) : m_ReadQueue{1024}, m_Info{std::move(info)}
    {
      const auto device_id = reg_query(NETWORK_ADAPTERS);
      if (device_id == nullptr)
        throw std::invalid_argument{"cannot query registery"};
      std::stringstream ss;
      ss << "\\\\.\\Global\\" << device_id << ".tap";
      const auto fname = ss.str();
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
      LogInfo("setting addresses");
      // set addresses
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam != AF_INET)
        {
          LogError("invalid address family");
          throw std::invalid_argument{"address family not supported"};
        }
        DWORD len;
        IPADDR sock[3]{};
        const nuint32_t addr = xhtonl(net::TruncateV6(ifaddr.range.addr));
        const nuint32_t mask = xhtonl(net::TruncateV6(ifaddr.range.netmask_bits));

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
                m_Device, TAP_IOCTL_CONFIG_DHCP_MASQ, ep, sizeof(ep), ep, sizeof(ep), &len, NULL))
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
      ULONG flag = 1;
      DWORD len;
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

      m_Run = true;
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
      // close the reader thread
      CloseHandle(m_ReadThread);
    }

    int
    PollFD() const override
    {
      return -1;
    }

    bool
    HasNextPacket() override
    {
      return not m_ReadQueue.empty();
    }

    std::string
    Name() const override
    {
      return "win32 tun interface";
    }

    static DWORD FAR PASCAL
    Loop(void* u)
    {
      static_cast<Win32Interface*>(u)->ReadLoop();
      return 0;
    }

    void
    Start()
    {
      LogInfo("starting reader thread...");
      m_IOCP = CreateIoCompletionPort(m_Device, nullptr, (ULONG_PTR)this, 2);
      m_ReadThread = CreateThread(nullptr, 0, &Loop, this, 0, nullptr);
      LogInfo("reader thread up");
    }

    net::IPPacket
    ReadNextPacket()
    {
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
    ReadLoop()
    {
      {
        asio_evt_pkt* ev = new asio_evt_pkt{true};
        ev->Read(m_Device);
      }
      while (m_Run)
      {
        DWORD size;
        ULONG_PTR user;
        OVERLAPPED* ovl = nullptr;
        if (not GetQueuedCompletionStatus(m_IOCP, &size, &user, &ovl, 100))
          continue;
        asio_evt_pkt* pkt = (asio_evt_pkt*)ovl;
        LogDebug("got iocp event size=", size, " read=", pkt->read);
        if (pkt->read)
        {
          if (size)
          {
            pkt->pkt.sz = size;
            m_ReadQueue.pushBack(pkt->pkt);
          }
        }
        delete pkt;
        pkt = new asio_evt_pkt{true};
        pkt->Read(m_Device);
      }
    }
  };

  class Win32Platform : public Platform
  {
   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) override
    {
      auto netif = std::make_shared<Win32Interface>(std::move(info));
      netif->Start();
      return netif;
    };
  };

}  // namespace llarp::vpn

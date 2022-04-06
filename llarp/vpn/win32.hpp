#pragma once

// define this because wintun is retaded ?
#define _Post_maybenull_
#include <wintun.h>
#undef _Post_maybenull_
#include <iphlpapi.h>
#include <netioapi.h>
#include <io.h>
#include <fcntl.h>
#include <llarp/util/str.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/win32/exception.hpp>
#include <llarp/win32/fwpm.hpp>

#include <llarp/crypto/crypto.hpp>

namespace llarp::vpn
{
  namespace
  {
    std::shared_ptr<MIB_IPFORWARD_TABLE2>
    GetIPForwards()
    {
      MIB_IPFORWARD_TABLE2* table = nullptr;
      if (auto err = GetIpForwardTable2(AF_UNSPEC, &table); err != ERROR_SUCCESS)
        throw win32::error{err, "cannot get ip forwad table: "};
      return std::shared_ptr<MIB_IPFORWARD_TABLE2>{table, [](auto* ptr) { FreeMibTable(ptr); }};
    }

    std::unique_ptr<IP_ADAPTER_ADDRESSES_LH[]>
    GetAdapterTable()
    {
      DWORD sz{};
      if (auto err = GetAdaptersAddresses(
              AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, nullptr, &sz);
          err != ERROR_BUFFER_OVERFLOW)
        throw win32::error{err, "cannot allocate adapter addresses: "};

      std::unique_ptr<IP_ADAPTER_ADDRESSES_LH[]> table{new IP_ADAPTER_ADDRESSES_LH[sz]};
      if (auto err = GetAdaptersAddresses(
              AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, table.get(), &sz);
          err != ERROR_SUCCESS)
        throw win32::error{err, "cannot get adapter addresses: "};

      return table;
    }

  }  // namespace

  bool
  operator==(const IP_ADDRESS_PREFIX& prefix, const InterfaceAddress& addr)
  {
    if (prefix.Prefix.si_family != addr.fam)
      return false;
    IPRange range{};
    if (prefix.Prefix.si_family == AF_INET)
    {
      range = IPRange{
          net::ExpandV4(ToHost(nuint32_t{prefix.Prefix.Ipv4.sin_addr.s_addr})),
          net::ExpandV4(netmask_ipv4_bits(prefix.PrefixLength))};
    }
    else if (prefix.Prefix.si_family == AF_INET6)
    {
      range = IPRange{
          net::In6ToHUInt(prefix.Prefix.Ipv6.sin6_addr), netmask_ipv6_bits(prefix.PrefixLength)};
    }
    else
      return false;
    return range == addr.range;
  }

  bool
  operator!=(const IP_ADDRESS_PREFIX& prefix, const InterfaceAddress& addr)
  {
    return not(prefix == addr);
  }

  namespace wintun
  {
    template <typename T>
    struct Deleter
    {
      std::function<void(T*)> _close;
      Deleter(std::function<void(T*)> func) : _close{func}
      {}

      void
      operator()(T* ptr) const
      {
        if (ptr)
          _close(ptr);
      }
    };

    struct Packet
    {
      std::unique_ptr<BYTE[], Deleter<BYTE>> _data;
      DWORD _sz;

      explicit Packet(BYTE* pkt, DWORD sz, Deleter<BYTE> deleter)
          : _data{pkt, std::move(deleter)}, _sz{sz}
      {}

      llarp_buffer_t
      ConstBuffer() const
      {
        return llarp_buffer_t{_data.get(), _sz};
      }
    };

    using Packet_ptr = std::shared_ptr<Packet>;
    using Adapter_ptr = std::shared_ptr<_WINTUN_ADAPTER>;
    using Session_ptr = std::shared_ptr<_TUN_SESSION>;

    /// @brief given a container of data hash it and make it into a GUID so we have a way to
    /// deterministically generate GUIDs
    template <typename Data>
    static GUID
    MakeDeterministicGUID(Data data)
    {
      ShortHash h{};
      CryptoManager::instance()->shorthash(h, llarp_buffer_t{data});
      GUID guid{};
      std::copy_n(
          h.begin(), std::min(sizeof(GUID), sizeof(ShortHash)), reinterpret_cast<uint8_t*>(&guid));
      return guid;
    }

    struct API
    {
      const HMODULE _handle;
      WINTUN_CREATE_ADAPTER_FUNC _createAdapter;
      WINTUN_OPEN_ADAPTER_FUNC _openAdapter;
      WINTUN_GET_ADAPTER_NAME_FUNC _getAdapterName;
      WINTUN_DELETE_ADAPTER_FUNC _deleteAdapter;
      WINTUN_FREE_ADAPTER_FUNC _freeAdapter;

      WINTUN_START_SESSION_FUNC _startSession;
      WINTUN_END_SESSION_FUNC _endSession;

      WINTUN_ENUM_ADAPTERS_FUNC _iterAdapters;

      WINTUN_GET_ADAPTER_LUID_FUNC _getAdapterLUID;

      WINTUN_RECEIVE_PACKET_FUNC _readPacket;
      WINTUN_RELEASE_RECEIVE_PACKET_FUNC _releaseRead;

      WINTUN_ALLOCATE_SEND_PACKET_FUNC _allocWrite;
      WINTUN_SEND_PACKET_FUNC _writePacket;

      WINTUN_DELETE_POOL_DRIVER_FUNC _deletePool;
      WINTUN_SET_LOGGER_FUNC _setLogger;

      static constexpr auto PoolName = L"Lokinet";

      explicit API(const char* wintunlib = "wintun.dll") : _handle{LoadLibrary(wintunlib)}
      {
        if (not _handle)
          throw llarp::win32::error{"failed to open library " + std::string{wintunlib} + ": "};

        const std::map<std::string, FARPROC*> funcs{
            {"WintunEnumAdapters", (FARPROC*)&_iterAdapters},
            {"WintunOpenAdapter", (FARPROC*)&_openAdapter},
            {"WintunCreateAdapter", (FARPROC*)&_createAdapter},
            {"WintunDeleteAdapter", (FARPROC*)&_deleteAdapter},
            {"WintunFreeAdapter", (FARPROC*)&_freeAdapter},
            {"WintunGetAdapterName", (FARPROC*)&_getAdapterName},
            {"WintunStartSession", (FARPROC*)&_startSession},
            {"WintunEndSession", (FARPROC*)&_endSession},
            {"WintunGetAdapterLUID", (FARPROC*)&_getAdapterLUID},
            {"WintunReceivePacket", (FARPROC*)&_readPacket},
            {"WintunReleaseReceivePacket", (FARPROC*)&_releaseRead},
            {"WintunSendPacket", (FARPROC*)&_writePacket},
            {"WintunAllocateSendPacket", (FARPROC*)&_allocWrite},
            {"WintunSetLogger", (FARPROC*)&_setLogger},
            {"WintunDeletePoolDriver", (FARPROC*)&_deletePool}};

        for (auto& [procname, ptr] : funcs)
        {
          if (FARPROC funcptr = GetProcAddress(_handle, procname.c_str()))
            *ptr = funcptr;
          else
            throw llarp::win32::error{"could not find function " + procname + ": "};
        }

        // set wintun logger function
        _setLogger([](WINTUN_LOGGER_LEVEL lvl, LPCWSTR _msg) {
          std::string msg = to_width<std::string>(std::wstring{_msg});
          switch (lvl)
          {
            case WINTUN_LOG_INFO:
              LogInfo(">> ", msg);
              return;
            case WINTUN_LOG_WARN:
              LogWarn(">> ", msg);
              return;
            case WINTUN_LOG_ERR:
              LogError(">> ", msg);
              return;
          }
        });
      }

      ~API()
      {
        FreeLibrary(_handle);
      }

      /// @brief remove all existing adapters on our pool
      void
      RemoveAllAdapters()
      {
        _iterAdapters(
            PoolName,
            [](WINTUN_ADAPTER_HANDLE handle, LPARAM user) -> int {
              API* self = (API*)user;

              LogInfo("deleting adapter: ", self->GetAdapterName(handle));

              self->_deleteAdapter(handle, false, nullptr);
              return 1;
            },
            (LPARAM)this);
      }

      /// @brief called when we uninstall the lokinet service to remove all traces of wintun
      void
      CleanUpForUninstall()
      {
        LogInfo("cleaning up all our wintun jizz...");
        _deletePool(PoolName, nullptr);
        LogInfo("wintun jizz should be gone now");
      }

      [[nodiscard]] auto
      GetAdapterUID(const Adapter_ptr& adapter)
      {
        NET_LUID _uid{};
        _getAdapterLUID(adapter.get(), &_uid);
        return _uid;
      }

      [[nodiscard]] auto
      GetInterfaceIndex(const Adapter_ptr& adapter)
      {
        const auto luid = GetAdapterUID(adapter);
        NET_IFINDEX index{};
        if (auto err = ConvertInterfaceLuidToIndex(&luid, &index); err != NO_ERROR)
          throw win32::error{err, "cannot get interface index"};
        return index;
      }

      [[nodiscard]] auto
      ReadPacket(const Session_ptr& session)
      {
        DWORD sz{};
        if (auto* pkt = _readPacket(session.get(), &sz))
        {
          return std::make_shared<Packet>(pkt, sz, Deleter<BYTE>{[session, this](auto* pkt) {
                                            _releaseRead(session.get(), pkt);
                                          }});
        }
        if (auto err = GetLastError(); err == ERROR_NO_MORE_ITEMS)
        {
          SetLastError(0);
        }
        else
          throw win32::error{"failed to read packet: "};
        return std::shared_ptr<Packet>{nullptr};
      }

      void
      WritePacket(const Session_ptr& session, const net::IPPacket& pkt)
      {
        if (auto* ptr = _allocWrite(session.get(), pkt.sz))
        {
          std::copy_n(pkt.buf, pkt.sz, ptr);
          _writePacket(session.get(), ptr);
          return;
        }
        // clear errors
        if (auto err = GetLastError(); err == ERROR_BUFFER_OVERFLOW)
        {
          SetLastError(0);
        }
        else
          throw win32::error{"failed to write packet: "};
      }

      [[nodiscard]] auto
      MakeAdapterPtr(const InterfaceInfo& info) const
      {
        const auto name = to_width<std::wstring>(info.ifname);

        Deleter<_WINTUN_ADAPTER> deleter{[this, name = info.ifname](auto* ptr) {
          LogInfo("deleting adapter ", name);
          _freeAdapter(ptr);
        }};

        if (auto ptr = _openAdapter(PoolName, name.c_str()))
          return Adapter_ptr{ptr, std::move(deleter)};

        llarp::LogInfo("creating new wintun adapter: ", info.ifname);

        // reset error code
        SetLastError(0);

        const GUID adapterGUID = MakeDeterministicGUID(info.ifname);

        if (auto ptr = _createAdapter(PoolName, name.c_str(), &adapterGUID, nullptr))
        {
          return Adapter_ptr{ptr, std::move(deleter)};
        }

        throw llarp::win32::error{"could not create adapter: "};
      }

      /// @brief make a wintun session smart pointer on a wintun adapter
      [[nodiscard]] auto
      MakeSessionPtr(const Adapter_ptr& adapter) const
      {
        if (auto ptr = _startSession(adapter.get(), WINTUN_MAX_RING_CAPACITY))
          return Session_ptr{ptr, _endSession};
        throw llarp::win32::error{"could not open session: "};
      }

      /// @brief get the name of an adapter
      [[nodiscard]] std::string
      GetAdapterName(WINTUN_ADAPTER_HANDLE adapter) const
      {
        std::wstring buf(MAX_ADAPTER_NAME, '\x00');
        if (_getAdapterName(adapter, buf.data()))
          return to_width<std::string>(buf);
        throw win32::error{"cannot get adapter name: "};
      }

      /// @brief add an interface address to a wintun adapter
      void
      AddAdapterAddress(const Adapter_ptr& adapter, const InterfaceAddress& addr)
      {
        const auto name = GetAdapterName(adapter.get());
        MIB_UNICASTIPADDRESS_ROW AddressRow;
        InitializeUnicastIpAddressEntry(&AddressRow);
        _getAdapterLUID(adapter.get(), &AddressRow.InterfaceLuid);
        AddressRow.OnLinkPrefixLength = addr.range.HostmaskBits();
        AddressRow.Address.si_family = addr.fam;
        switch (addr.fam)
        {
          case AF_INET:
            AddressRow.Address.Ipv4.sin_family = addr.fam;
            AddressRow.Address.Ipv4.sin_addr.S_un.S_addr =
                ToNet(net::TruncateV6(addr.range.addr)).n;
            break;
          case AF_INET6:
            LogInfo("skipping ipv6 address assignment on ", name);
            return;
          default:
            throw std::invalid_argument{llarp::stringify("invalid address family: ", addr.fam)};
        }
        AddressRow.DadState = IpDadStatePreferred;
        LogInfo("setting address ", addr.range, " on ", name);
        if (auto err = CreateUnicastIpAddressEntry(&AddressRow); err != ERROR_SUCCESS)
          throw llarp::win32::error{err, "failed to set interface address: "};
      }

      void
      DelAdapterAddress(const Adapter_ptr& adapter, const InterfaceAddress& addr)
      {
        const auto uid = GetAdapterUID(adapter);
        auto table = GetIPForwards();
        for (ULONG idx = 0; idx < table->NumEntries; ++idx)
        {
          if (table->Table[idx].InterfaceLuid.Value != uid.Value)
            continue;

          if (table->Table[idx].DestinationPrefix != addr)
            continue;

          if (auto err = DeleteIpForwardEntry2(&table->Table[idx]); err != ERROR_SUCCESS)
            throw win32::error{err, "failed to remove interface address: "};
        }
      }
    };

    /// @brief execute a shell command
    static void
    Exec(std::wstring wcmd)
    {
      LogInfo("[win32 exec]: ", to_width<std::string>(wcmd));
      ::_wsystem(wcmd.c_str());
    }

    /// @brief get path to win32 system directory
    static std::wstring
    SysrootPath()
    {
      wchar_t path[MAX_PATH] = {0};
      if (not GetSystemDirectoryW(path, _countof(path)))
        return L"C:\\windows\\system32";
      return path;
    }

    /// @brief get path to netsh.exe
    static std::wstring
    NetshExe()
    {
      return SysrootPath() + L"\\netsh.exe";
    }

    /// @brief executes netsh.exe with arguments
    static void
    NetSH(std::wstring args)
    {
      Exec(NetshExe() + L" " + args);
    }

    static void
    RouteExec(std::string args)
    {
      Exec(SysrootPath() + L"\\route.exe" + L" " + to_width<std::wstring>(args));
    }

    static void
    SetDNS(std::wstring adapter, llarp::SockAddr nameserver)
    {
      wintun::NetSH(
          std::wstring{L"interface "} + (nameserver.isIPv4() ? L"ipv4" : L"ipv6")
          + L" set dnsservers name=\"" + adapter + L"\" source=static \""
          + to_width<std::wstring>(nameserver.hostString()) + L" primary");
    }

    /// @brief raii wrapper that sets dns settings to an original state on destruction
    struct DNSRevert
    {
      DNSRevert() = default;

      explicit DNSRevert(std::wstring _ifname, llarp::SockAddr _nameserver)
          : ifname{std::move(_ifname)}, nameserver{std::move(_nameserver)}
      {}
      const std::wstring ifname;
      const llarp::SockAddr nameserver;

      ~DNSRevert()
      {
        if (not ifname.empty())
        {
          SetDNS(ifname, nameserver);
        }
      }
    };

  }  // namespace wintun

  class WintunInterface : public NetworkInterface
  {
    wintun::API* const m_API;
    wintun::Session_ptr m_Session;
    wintun::Adapter_ptr m_Adapter;
    const InterfaceInfo m_Info;
    AbstractRouter* const _router;
    std::vector<wintun::DNSRevert> m_RevertDNS;

   public:
    explicit WintunInterface(wintun::API* api, InterfaceInfo info, AbstractRouter* router)
        : m_API{api}, m_Info{std::move(info)}, _router{router}
    {}

    [[nodiscard]] NET_IFINDEX
    InterfaceIndex() const
    {
      return m_API->GetInterfaceIndex(m_Adapter);
    }

    void
    AddAddressRange(IPRange range)
    {
      m_API->AddAdapterAddress(
          m_Adapter, InterfaceAddress{range, range.IsV4() ? AF_INET : AF_INET6});
    }

    void
    DelAddressRange(IPRange range)
    {
      m_API->DelAdapterAddress(
          m_Adapter, InterfaceAddress{range, range.IsV4() ? AF_INET : AF_INET6});
    }

    /// @brief start wintun session
    /// this is a separate function so that if the constructor throws we don't call this
    void
    Start()
    {
      // prepare dns revert
      {
        auto table = GetAdapterTable();
        for (auto* ent = table.get(); ent->Next; ent = ent->Next)
        {
          if (auto* dns = ent->FirstDnsServerAddress;
              dns and dns->Address.iSockaddrLength and dns->Address.lpSockaddr)
          {
            auto* addr = static_cast<SOCKADDR*>(dns->Address.lpSockaddr);
            llarp::SockAddr saddr{};
            switch (addr->sa_family)
            {
              case AF_INET:
                saddr = *reinterpret_cast<sockaddr_in*>(addr);
                break;
              case AF_INET6:
                saddr = *reinterpret_cast<sockaddr_in6*>(addr);
                break;
              default:
                continue;
            }
            m_RevertDNS.emplace_back(to_width<std::wstring>(std::to_string(ent->IfIndex)), saddr);
          }
        }
      };

      // make our adapter
      m_Adapter = m_API->MakeAdapterPtr(m_Info);

      // set our interface addresses
      for (const auto& addr : m_Info.addrs)
      {
        m_API->AddAdapterAddress(m_Adapter, addr);
      }

      const llarp::SockAddr dns{m_Info.dnsaddr};

      // set dns
      for (const auto& ent : m_RevertDNS)
        wintun::SetDNS(ent.ifname, dns);
      wintun::SetDNS(to_width<std::wstring>(std::to_string(InterfaceIndex())), dns);

      m_Session = m_API->MakeSessionPtr(m_Adapter);
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

    std::string
    InterfaceAddressStringV4() const
    {
      return net::TruncateV6(m_Info.addrs.begin()->range.addr).ToString();
    }

    std::string
    InterfaceAddressStringV6() const
    {
      return m_Info.addrs.begin()->range.ToString();
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt{};

      if (auto ptr = m_API->ReadPacket(m_Session))
      {
        if (not pkt.Load(ptr->ConstBuffer()))
        {
          pkt.sz = 0;
        }
      }

      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      m_API->WritePacket(m_Session, pkt);
      return true;
    }

    NET_LUID
    GetUID() const
    {
      return m_API->GetAdapterUID(m_Adapter);
    }
  };

  class Win32RouteManager : public IRouteManager
  {
   public:
    void
    AddRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t) override
    {
      const auto ip_str = std::visit([](auto&& ip) { return ip.ToString(); }, ip);
      const auto gateway_str = std::visit([](auto&& ip) { return ip.ToString(); }, gateway);
      wintun::RouteExec("ADD " + ip_str + " MASK 255.255.255.255 " + gateway_str + " METRIC 2");
    }

    void
    DelRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t) override
    {
      const auto ip_str = std::visit([](auto&& ip) { return ip.ToString(); }, ip);
      const auto gateway_str = std::visit([](auto&& ip) { return ip.ToString(); }, gateway);
      wintun::RouteExec("DELETE " + ip_str + " MASK 255.255.255.255 " + gateway_str + " METRIC 2");
    }

    std::vector<IPVariant_t>
    GetGatewaysNotOnInterface(NetworkInterface& vpn) override
    {
      const auto uid = dynamic_cast<WintunInterface&>(vpn).GetUID();
      std::vector<IPVariant_t> gateways;
      auto table = GetIPForwards();
      for (ULONG idx = 0UL; idx < table->NumEntries; ++idx)
      {
        if (table->Table[idx].InterfaceLuid.Value == uid.Value)
          continue;
        if (table->Table[idx].DestinationPrefix.PrefixLength)
          continue;
        if (table->Table[idx].NextHop.si_family == AF_INET)
          gateways.emplace_back(ToHost(nuint32_t{table->Table[idx].NextHop.Ipv4.sin_addr.s_addr}));
        else if (table->Table[idx].NextHop.si_family == AF_INET6)
          gateways.emplace_back(net::In6ToHUInt(table->Table[idx].NextHop.Ipv6.sin6_addr));
      };
      return gateways;
    }

    void
    AddDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      // add loopback as exception as god is dead
      wintun::RouteExec("ADD 127.0.0.0 255.0.0.0 0.0.0.0");
      IRouteManager::AddDefaultRouteViaInterface(vpn);
    }

    void
    DelDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      IRouteManager::DelDefaultRouteViaInterface(vpn);
      // remove loopback exception and pray for forgiveness
      wintun::RouteExec("REMOVE 127.0.0.0 255.0.0.0 0.0.0.0");
    }

    void
    ModifyRouteViaInterface(std::string modifier, NetworkInterface& iface, IPRange range)
    {
      if (range.IsV4())
      {
        const auto netif_str =
            dynamic_cast<const WintunInterface&>(iface).InterfaceAddressStringV4();
        wintun::RouteExec(
            modifier + " " + range.BaseAddressString() + " MASK "
            + netmask_ipv4_bits(range.HostmaskBits()).ToString() + " " + netif_str + " METRIC 2");
      }
      else
      {
        const auto netif_str =
            dynamic_cast<const WintunInterface&>(iface).InterfaceAddressStringV6();
        const auto interface_str =
            std::to_string(dynamic_cast<const WintunInterface&>(iface).InterfaceIndex());
        wintun::RouteExec(
            modifier + " " + range.ToString() + " " + netif_str + " IF " + interface_str
            + " METRIC 2");
      }
    }

    void
    AddRouteViaInterface(NetworkInterface& iface, IPRange range) override
    {
      ModifyRouteViaInterface("ADD", iface, range);
    }
    void
    DelRouteViaInterface(NetworkInterface& iface, IPRange range) override
    {
      ModifyRouteViaInterface("REMOVE", iface, range);
    }
  };

  class Win32Platform : public wintun::API, public Platform, public Win32RouteManager
  {
   public:
    Win32Platform() : API{}, Platform{}, Win32RouteManager{}
    {
      RemoveAllAdapters();
    }

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override
    {
      auto adapter = std::make_shared<WintunInterface>(this, info, router);
      adapter->Start();
      return adapter;
    };

    IRouteManager&
    RouteManager() override
    {
      return *this;
    }
  };

}  // namespace llarp::vpn

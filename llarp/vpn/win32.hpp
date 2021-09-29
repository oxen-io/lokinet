#pragma once

#define _Post_maybenull_

#include <wintun.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <io.h>
#include <fcntl.h>
#include <llarp/util/str.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/router/abstractrouter.hpp
#include <llarp/win32/exception.hpp>
#include <llarp/win32/fwpm.hpp>

namespace llarp::vpn
{
  namespace
  {
    /// convert between std::string or std::wstring
    /// XXX: this function is shit
    template <typename OutStr, typename InStr>
    OutStr
    to_width(const InStr& str)
    {
      OutStr ostr{};
      typename OutStr::value_type buf[2] = {};

      for (const auto& ch : str)
      {
        buf[0] = static_cast<typename OutStr::value_type>(ch);
        ostr.append(buf);
      }
      return ostr;
    }

    template <typename Visit>
    void
    ForEachWIN32Interface(Visit visit)
    {
      DWORD dwSize{};
      if (GetIpForwardTable(nullptr, &dwSize, 0) != ERROR_INSUFFICIENT_BUFFER)
        throw llarp::win32::last_error{};

      auto table = std::make_unique<MIB_IPFORWARDTABLE[]>(dwSize / sizeof(MIB_IPFORWARDTABLE));
      MIB_IPFORWARDTABLE* ptr = table.get();
      if (GetIpForwardTable(ptr, &dwSize, 0) != NO_ERROR)
        throw llarp::win32::last_error{};

      for (DWORD idx = 0; idx < ptr->dwNumEntries; idx++)
        visit(&ptr->table[idx]);
    }

    std::unique_ptr<IP_ADAPTER_ADDRESSES_LH[]>
    AdapterTable()
    {
      DWORD sz{};
      if (auto err = GetAdaptersAddresses(
              AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, nullptr, &sz);
          err != ERROR_BUFFER_OVERFLOW)
        throw win32::error{err, "cannot allocate adapter addresses: "};

      auto table =
          std::make_unique<IP_ADAPTER_ADDRESSES_LH[]>(sz / sizeof(IP_ADAPTER_ADDRESSES_LH));
      if (auto err = GetAdaptersAddresses(
              AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, table.get(), &sz);
          err != ERROR_SUCCESS)
        throw win32::error{err, "cannot get adapter addresses: "};

      return table;
    }

  }  // namespace

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

    using Packet_ptr = std::unique_ptr<Packet>;
    using Adapter_ptr = std::shared_ptr<_WINTUN_ADAPTER>;
    using Session_ptr = std::shared_ptr<_TUN_SESSION>;

    struct API
    {
      WINTUN_CREATE_ADAPTER_FUNC _createAdapter;
      WINTUN_OPEN_ADAPTER_FUNC _openAdapter;
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

      static constexpr auto PoolName = L"Lokinet";

      explicit API(const char* wintunlib)
      {
        auto handle = ::LoadLibrary(wintunlib);
        if (handle == nullptr)
          throw llarp::win32::last_error{"failed to open library " + std::string{wintunlib} + ": "};

        const std::map<std::string, FARPROC*> funcs{
            {"WintunEnumAdapters", (FARPROC*)&_iterAdapters},
            {"WintunOpenAdapter", (FARPROC*)&_openAdapter},
            {"WintunCreateAdapter", (FARPROC*)&_createAdapter},
            {"WintunDeleteAdapter", (FARPROC*)&_deleteAdapter},
            {"WintunFreeAdapter", (FARPROC*)&_freeAdapter},
            {"WintunStartSession", (FARPROC*)&_startSession},
            {"WintunEndSession", (FARPROC*)&_endSession},
            {"WintunGetAdapterLUID", (FARPROC*)&_getAdapterLUID},
            {"WintunReceivePacket", (FARPROC*)&_readPacket},
            {"WintunReleaseReceivePacket", (FARPROC*)&_releaseRead},
            {"WintunSendPacket", (FARPROC*)&_writePacket},
            {"WintunAllocateSendPacket", (FARPROC*)&_allocWrite},
        };

        for (auto& [procname, ptr] : funcs)
        {
          if (FARPROC funcptr = GetProcAddress(handle, procname.c_str()))
            *ptr = funcptr;
          else
            throw llarp::win32::last_error{"could not find function " + procname + ": "};
        }

        // remove all existing adapters
        _iterAdapters(
            PoolName,
            [](WINTUN_ADAPTER_HANDLE handle, LPARAM user) -> int {
              ((API*)(user))->_deleteAdapter(handle, true, nullptr);
              return TRUE;
            },
            (LPARAM)this);
      }

      [[nodiscard]] auto
      ReadPacket(const Session_ptr& session)
      {
        Packet_ptr ptr;
        DWORD sz{};
        if (auto* pkt = _readPacket(session.get(), &sz))
        {
          ptr = std::make_unique<Packet>(
              pkt, sz, Deleter<BYTE>{[ptr = session.get(), this](auto* pkt) {
                _releaseRead(ptr, pkt);
              }});
        }
        else  // clear last error
          SetLastError(0);
        return ptr;
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
        SetLastError(0);
      }

      [[nodiscard]] auto
      MakeAdapterPtr(const InterfaceInfo& info) const
      {
        const auto name = to_width<std::wstring>(info.ifname);

        Deleter<_WINTUN_ADAPTER> deleter{[this](auto* ptr) {
          _deleteAdapter(ptr, true, nullptr);
          _freeAdapter(ptr);
        }};

        if (auto ptr = _openAdapter(PoolName, name.c_str()))
          return Adapter_ptr{ptr, std::move(deleter)};
        // reset error code
        SetLastError(0);

        if (auto ptr = _createAdapter(PoolName, name.c_str(), nullptr, nullptr))
          return Adapter_ptr{ptr, std::move(deleter)};

        throw llarp::win32::last_error{"could not create adapter: "};
      }

      [[nodiscard]] auto
      MakeSessionPtr(const Adapter_ptr& adapter) const
      {
        if (auto ptr = _startSession(adapter.get(), WINTUN_MAX_RING_CAPACITY))
          return Session_ptr{ptr, _endSession};
        throw llarp::win32::last_error{"could not open session: "};
      }

      void
      AddAdapterAddress(const Adapter_ptr& adapter, const InterfaceAddress& addr)
      {
        MIB_UNICASTIPADDRESS_ROW AddressRow{};
        InitializeUnicastIpAddressEntry(&AddressRow);
        _getAdapterLUID(adapter.get(), &AddressRow.InterfaceLuid);
        AddressRow.OnLinkPrefixLength = addr.range.HostmaskBits();
        switch (addr.fam)
        {
          case AF_INET:
            AddressRow.Address.Ipv4.sin_family = addr.fam;
            AddressRow.Address.Ipv4.sin_addr.S_un.S_addr =
                ToNet(net::TruncateV6(addr.range.addr)).n;
            break;
          case AF_INET6:
            AddressRow.Address.Ipv6.sin6_family = addr.fam;
            AddressRow.Address.Ipv6.sin6_addr = net::HUIntToIn6(addr.range.addr);
            break;
          default:
            throw std::invalid_argument{llarp::stringify("invalid address family: ", addr.fam)};
        }
        AddressRow.DadState = IpDadStatePreferred;
        if (auto err = CreateUnicastIpAddressEntry(&AddressRow); err != ERROR_SUCCESS)
          throw llarp::win32::error{err, "failed to set interface address: "};
      }
    };

    static void
    Exec(std::wstring wcmd)
    {
      LogInfo("[win32 exec]: ", to_width<std::string>(wcmd));
      ::_wsystem(wcmd.c_str());
    }

    static std::wstring
    SysrootPath()
    {
      wchar_t path[MAX_PATH] = {0};
      if (not GetSystemDirectoryW(path, _countof(path)))
        return L"C:\\windows\\system32";
      return path;
    }

    static std::wstring
    NetshExe()
    {
      return SysrootPath() + L"\\netsh.exe";
    }

    static void
    NetSH(std::wstring cmd)
    {
      Exec(NetshExe() + L" " + cmd);
    }

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
          NetSH(
              std::wstring{L"interface "} + (nameserver.isIPv4() ? L"ipv4" : L"ipv6")
              + L" set dns \"" + ifname + L"\" \"" + to_width<std::wstring>(nameserver.hostString())
              + L"\"");
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

    void
    SetAdapterDNS(std::wstring adapter, llarp::SockAddr nameserver)
    {
      // netsh interface ipv{4,6} set dns "$adapter" static "$ip" primary
      wintun::NetSH(
          std::wstring{L"interface "} + (nameserver.isIPv4() ? L"ipv4" : L"ipv6") + L" set dns \""
          + adapter + L"\" static ip \"" + to_width<std::wstring>(nameserver.hostString())
          + L"\" primary");
    }

   public:
    explicit WintunInterface(wintun::API* api, InterfaceInfo info, AbstractRouter* router)
        : m_API{api}, m_Info{std::move(info)}, _router{router}
    {
      auto table = AdapterTable();
      // get existing dns settings so we can restore them when we destruct
      std::unordered_map<std::wstring, llarp::SockAddr> dnsAddrs;

      for (auto* row = table.get(); row->Next; row = row->Next)
      {
        if (row->FirstDnsServerAddress and row->FriendlyName)
        {
          LogInfo(to_width<std::string>(std::wstring{row->FriendlyName}));
          dnsAddrs.emplace(
              row->FriendlyName,
              *static_cast<const sockaddr*>(row->FirstDnsServerAddress->Address.lpSockaddr));
        }
      }

      // make our adapter
      m_Adapter = m_API->MakeAdapterPtr(m_Info);

      // set our interface addresses
      for (const auto& addr : m_Info.addrs)
      {
        m_API->AddAdapterAddress(m_Adapter, addr);
      }

      // set new dns settings on every adapter
      for (auto* row = table.get(); row->Next; row = row->Next)
      {
        if (row->FriendlyName)
        {
          SetAdapterDNS(row->FriendlyName, m_Info.dnsaddr);
        }
      }
      // set up dns reverting
      for (const auto& [ifaddr, dnsaddr] : dnsAddrs)
        m_RevertDNS.emplace_back(ifaddr, dnsaddr);
    }

    /// @brief start wintun session
    /// this is a separate function so that if the constructor throws we don't call this
    void
    Start()
    {
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
  };

  /// \brief FWPMRouterManager is heavily based off wireguard's windows port's firewall code
  class FWPMRouteManager : public IRouteManager
  {
    HANDLE m_Handle;

    template <typename Operation>
    void
    RunTransaction(Operation tx)
    {
      if (auto err = FwpmTransactionBegin0(m_Handle, 0); err != ERROR_SUCCESS)
      {
        throw std::runtime_error{"FwpmTransactionBegin0 failed"};
      }
      try
      {
        tx();
        if (auto err = FwpmTransactionCommit0(m_Handle); err != ERROR_SUCCESS)
        {
          throw std::runtime_error{"FwpmTransactionCommit0 failed"};
        }
      }
      catch (std::exception& ex)
      {
        LogError("failed to run fwpm transaction: ", ex.what());
        FwpmTransactionAbort0(m_Handle);
        throw std::current_exception();
      }
    }

    static void
    GenerateUUID(UUID& uuid)
    {
      if (auto err = UuidCreateSequential(&uuid); err != RPC_S_OK)
      {
        throw std::runtime_error{"cannot generate uuid"};
      }
    }

    class Provider
    {
      std::wstring _name;
      std::wstring _description;

     protected:
      FWPM_PROVIDER0 _provider;

     public:
      explicit Provider(const wchar_t* name, const wchar_t* description)
          : _name{name}, _description{description}
      {
        GenerateUUID(_provider.providerKey);
        _provider.displayData.name = _name.data();
        _provider.displayData.description = _description.data();
        _provider.flags = 0;
        _provider.serviceName = nullptr;
      }

      operator FWPM_PROVIDER0*()
      {
        return &_provider;
      }
    };

    class SubLayer
    {
      std::wstring _name;
      std::wstring _description;

     protected:
      FWPM_SUBLAYER0 _sublayer;

     public:
      explicit SubLayer(const wchar_t* name, const wchar_t* description, GUID* providerKey)
          : _name{name}, _description{description}
      {
        GenerateUUID(_sublayer.subLayerKey);
        _sublayer.providerKey = providerKey;
        _sublayer.displayData.name = _name.data();
        _sublayer.displayData.description = _description.data();
        _sublayer.weight = 0xffff;
      }

      operator FWPM_SUBLAYER0*()
      {
        return &_sublayer;
      }
    };

    class Filter
    {
      std::wstring _name, _description;
      std::vector<FWPM_FILTER_CONDITION0_> _conditions;
      FWPM_FILTER0 _filter;
      HANDLE _engine;
      uint64_t _ID;

     public:
      explicit Filter(
          HANDLE engineHandle,
          FWPM_SUBLAYER0* sublayer,
          FWP_ACTION_TYPE action,
          GUID layerKey,
          std::vector<FWPM_FILTER_CONDITION0_> conditions,
          uint8_t weight,
          std::wstring name,
          std::wstring description)
          : _name{std::move(name)}
          , _description{std::move(description)}
          , _conditions{std::move(conditions)}
          , _engine{engineHandle}
      {
        _filter.layerKey = layerKey;
        _filter.action.type = action;
        _filter.subLayerKey = sublayer->subLayerKey;
        _filter.weight.uint8 = weight;
        _filter.weight.type = FWP_UINT8;
        _filter.numFilterConditions = _conditions.size();
        if (not conditions.empty())
          _filter.filterCondition = _conditions.data();

        _filter.displayData.name = _name.data();
        _filter.displayData.description = _description.data();
        if (auto err = FwpmFilterAdd0(_engine, &_filter, nullptr, &_ID); err != ERROR_SUCCESS)
          throw win32::error{err, "failed to add fwpm filter: "};
      }

      ~Filter()
      {
        FwpmFilterDeleteById0(_engine, _ID);
      }

      bool
      operator<(const Filter& other) const
      {
        return _ID < other._ID;
      }

      bool
      Matches(FWP_ACTION_TYPE type, const GUID layerKey) const
      {
        return _filter.action.type == type and _filter.layerKey == layerKey;
      }

      uint64_t
      ID() const
      {
        return _ID;
      }
    };

    class Firewall : public Provider, public SubLayer
    {
      HANDLE m_Handle;

      std::unordered_map<uint64_t, std::unique_ptr<Filter>> m_Filters;

     public:
      Firewall(HANDLE handle)
          : Provider{L"Lokinet", L"Lokinet Provider"}
          , SubLayer{L"Lokinet Filters", L"RoutePoker Filters", &_provider.providerKey}
          , m_Handle{handle}
      {
        if (auto err = FwpmProviderAdd0(m_Handle, *this, 0); err != ERROR_SUCCESS)
        {
          throw std::runtime_error{"fwpmProviderAdd0 failed"};
        }
        if (auto err = FwpmSubLayerAdd0(m_Handle, *this, 0); err != ERROR_SUCCESS)
        {
          throw std::runtime_error{"fwpmSubLayerAdd0 failed"};
        }
      }

      ~Firewall()
      {
        FwpmSubLayerDeleteByKey0(m_Handle, &_sublayer.subLayerKey);
        FwpmProviderDeleteByKey0(m_Handle, &_provider.providerKey);
      }

      uint64_t
      AddFilter(
          FWP_ACTION_TYPE action,
          GUID layerKey,
          uint8_t weight,
          std::vector<FWPM_FILTER_CONDITION0> conditions,
          std::wstring name,
          std::wstring description)
      {
        auto filter = std::make_unique<Filter>(
            m_Handle,
            *this,
            std::move(action),
            std::move(layerKey),
            std::move(conditions),
            std::move(weight),
            std::move(name),
            std::move(description));

        const auto id = filter->ID();
        m_Filters[id] = std::move(filter);
        return id;
      }

      void
      DropFilter(uint64_t id)
      {
        m_Filters.erase(id);
      }
    };

    std::unique_ptr<Firewall> m_Firewall;

    void
    PermitLoopback()
    {
      FWPM_FILTER_CONDITION0_ condition = win32::MakeCondition<huint32_t>(
          win32::FWPM_CONDITION_FLAGS(),
          FWP_MATCH_FLAGS_ALL_SET,
          huint32_t{FWP_CONDITION_FLAG_IS_LOOPBACK});

      m_Firewall->AddFilter(
          FWP_ACTION_PERMIT,
          win32::FWPM_LAYER_ALE_AUTH_CONNECT_V4(),
          13,
          {condition},
          L"Allow outbound v4 loopback",
          L"");

      m_Firewall->AddFilter(
          FWP_ACTION_PERMIT,
          win32::FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4(),
          13,
          {condition},
          L"Allow inbound v4 loopback",
          L"");

      m_Firewall->AddFilter(
          FWP_ACTION_PERMIT,
          win32::FWPM_LAYER_ALE_AUTH_CONNECT_V6(),
          13,
          {condition},

          L"Allow outbound v6 loopback",
          L"");

      m_Firewall->AddFilter(
          FWP_ACTION_PERMIT,
          win32::FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6(),
          13,
          {condition},

          L"Allow inbound v6 loopback",
          L"");
    }

    void
    PermitDHCP()
    {
      std::vector<FWPM_FILTER_CONDITION0_> outboundDHCP{};
      outboundDHCP.emplace_back(win32::MakeCondition<uint8_t>(
          win32::FWPM_CONDITION_IP_PROTOCOL(), FWP_MATCH_EQUAL, 0x11));

      outboundDHCP.emplace_back(win32::MakeCondition<huint16_t>(
          win32::FWPM_CONDITION_IP_LOCAL_PORT(), FWP_MATCH_EQUAL, huint16_t{68}));

      outboundDHCP.emplace_back(win32::MakeCondition<huint16_t>(
          win32::FWPM_CONDITION_IP_REMOTE_PORT(), FWP_MATCH_EQUAL, huint16_t{69}));

      outboundDHCP.emplace_back(win32::MakeCondition<huint32_t>(
          win32::FWPM_CONDITION_IP_REMOTE_ADDRESS(),
          FWP_MATCH_EQUAL,
          ipaddr_ipv4_bits(255, 255, 255, 255)));
      ;
    }

    void
    PermitTunInterface()
    {}

   public:
    FWPMRouteManager()
    {
      if (auto result = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, nullptr, &m_Handle);
          result != ERROR_SUCCESS)
      {
        throw std::runtime_error{"cannot open fwpm engine"};
      }
    }

    ~FWPMRouteManager()
    {
      FwpmEngineClose0(m_Handle);
    }

    void
    AddBlackhole() override
    {
      RunTransaction([&]() {
        m_Firewall = std::make_unique<Firewall>(m_Handle);
        PermitLoopback();
        PermitDHCP();
      });
    }

    void
    DelBlackhole() override
    {
      RunTransaction([&]() { m_Firewall.reset(); });
    }

    void
    AddRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t remoteport) override
    {}

    void
    DelRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t remoteport) override
    {}

    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {}

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {}

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
    {}

    void
    DelDefaultRouteViaInterface(std::string ifname) override
    {}
  };

  class Win32Platform : public wintun::API, public Platform
  {
    FWPMRouteManager _routeManager{};

   public:
    Win32Platform() : API{"wintun.dll"}, Platform{}
    {}

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override
    {
<<<<<<< HEAD
      return std::make_shared<WintunInterface>(this, info, router);
=======
      auto ptr = std::make_shared<WintunInterface>(this, info);
      ptr->Start();
      return ptr;
>>>>>>> 48f9abee8 (route poker updates)
    };

    IRouteManager&
    RouteManager() override
    {
      return _routeManager;
    }
  };

}  // namespace llarp::vpn

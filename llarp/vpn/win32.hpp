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
#include <fwpmu.h>

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
          err != ERROR_INSUFFICIENT_BUFFER)
        throw win32::error{err, "cannot get adapter addresses: "};

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

    template <typename Releaser>
    struct Packet
    {
      Releaser _release;
      BYTE* _data;
      DWORD _sz;

      explicit Packet(BYTE* pkt, DWORD sz, Releaser del)
          : _release{std::move(del)}, _data{pkt}, _sz{sz}
      {}

      llarp_buffer_t
      ConstBuffer() const
      {
        return llarp_buffer_t{_data, _sz};
      }

      ~Packet()
      {
        _release(_data);
      }
    };

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
        auto release = [ptr = session.get(), del = _releaseRead](auto* pkt) { del(ptr, pkt); };
        std::unique_ptr<Packet<decltype(release)>> ptr;
        DWORD sz{};
        if (auto* pkt = _readPacket(session.get(), &sz))
          ptr.reset(new Packet<decltype(release)>{pkt, sz, std::move(release)});
        return ptr;
      }

      void
      WritePacket(const Session_ptr& session, const net::IPPacket& pkt)
      {
        if (auto* ptr = _allocWrite(session.get(), pkt.sz))
        {
          std::copy_n(pkt.buf, pkt.sz, ptr);
          _writePacket(session.get(), ptr);
        }
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
        AddressRow.Address.Ipv4.sin_family = addr.fam;
        AddressRow.OnLinkPrefixLength = addr.range.HostmaskBits();
        switch (addr.fam)
        {
          case AF_INET:
            AddressRow.Address.Ipv4.sin_addr.S_un.S_addr =
                ToNet(net::TruncateV6(addr.range.addr)).n;
            break;
          case AF_INET6:
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

    struct DNSSettings
    {
      DNSSettings() = default;

      explicit DNSSettings(std::wstring _ifname, llarp::SockAddr _nameserver)
          : ifname{std::move(_ifname)}, nameserver{std::move(_nameserver)}
      {}
      std::wstring ifname;
      llarp::SockAddr nameserver;

      ~DNSSettings()
      {
        NetSH(
            std::wstring{L"interface "} + (nameserver.isIPv4() ? L"ipv4" : L"ipv6") + L" set dns \""
            + ifname + L"\" \"" + to_width<std::wstring>(nameserver.hostString()) + L"\"");
      }
    };

  }  // namespace wintun

  class WintunInterface : public NetworkInterface
  {
    wintun::API* const m_API;
    wintun::Adapter_ptr m_Adapter;
    wintun::Session_ptr m_Session;
    const InterfaceInfo m_Info;
    AbstractRouter* const _router;

    std::vector<wintun::DNSSettings> m_OldDNSSettings;
    std::thread m_ReaderThread;

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
    explicit WintunInterface(wintun::API* api, const InterfaceInfo& info, AbstractRouter* router)
        : m_API{api}, m_Info{info}, _router{router}
    {
      auto table = AdapterTable();
      // get existing dns settings so we can restore them when we destruct
      {
        auto* row = table.get();
        do
        {
          llarp::SockAddr saddr{
              *static_cast<const sockaddr*>(row->FirstDnsServerAddress->Address.lpSockaddr)};
          m_OldDNSSettings.emplace_back(
              to_width<std::wstring>(std::string{row->AdapterName}), std::move(saddr));
          row = row->Next;
        } while (row);
      }

      // make our adapter
      m_Adapter = m_API->MakeAdapterPtr(m_Info);

      // set our interface addresses
      for (const auto& addr : info.addrs)
      {
        m_API->AddAdapterAddress(m_Adapter, addr);
      }

      // set new dns settings on every adapter
      {
        auto* row = table.get();
        do
        {
          SetAdapterDNS(to_width<std::wstring>(std::string{row->AdapterName}), m_Info.dnsaddr);
          row = row->Next;
        } while (row);
      }

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

    class Firewall : public Provider, public SubLayer
    {
      HANDLE m_Handle;

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
    };

    std::unique_ptr<Firewall> m_Firewall;

    void
    PermitLokinetService()
    {}

    void
    PermitLoopback()
    {}

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
        PermitLokinetService();
        PermitLoopback();
      });
    }

    void
    DelBlackhole() override
    {
      RunTransaction([&]() { m_Firewall.reset(); });
    }

    void
    AddRoute(IPVariant_t ip, IPVariant_t gateway) override
    {}

    void
    DelRoute(IPVariant_t ip, IPVariant_t gateway) override
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
      return std::make_shared<WintunInterface>(this, info, router);
    };

    IRouteManager&
    RouteManager() override
    {
      return _routeManager;
    }
  };

}  // namespace llarp::vpn

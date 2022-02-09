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
      // make our adapter
      m_Adapter = m_API->MakeAdapterPtr(m_Info);

      // set our interface addresses
      for (const auto& addr : m_Info.addrs)
      {
        m_API->AddAdapterAddress(m_Adapter, addr);
      }
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
    InterfaceAddressString() const
    {
      return net::TruncateV6(m_Info.addrs.begin()->range.addr).ToString();
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
        throw win32::error{err, "FwpmTransactionBegin0 failed: "};
      }
      try
      {
        tx();
        if (auto err = FwpmTransactionCommit0(m_Handle); err != ERROR_SUCCESS)
        {
          throw win32::error{err, "FwpmTransactionCommit0 failed: "};
        }
      }
      catch (std::exception& ex)
      {
        LogError("failed to run fwpm transaction: ", ex.what());
        FwpmTransactionAbort0(m_Handle);
        throw std::runtime_error{ex.what()};
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
      FWPM_PROVIDER0 _provider;

     public:
      explicit Provider(const wchar_t* name, const wchar_t* description, DWORD flags = 0)
          : _name{name}, _description{description}, _provider{}
      {
        GenerateUUID(_provider.providerKey);
        _provider.displayData.name = _name.data();
        _provider.displayData.description = _description.data();
        _provider.flags = flags;
      }

      GUID& providerKey{_provider.providerKey};

      operator FWPM_PROVIDER0*()
      {
        return &_provider;
      }
    };

    class SubLayer
    {
      std::wstring _name;
      std::wstring _description;
      FWPM_SUBLAYER0 _sublayer;

     public:
      explicit SubLayer(const wchar_t* name, const wchar_t* description, GUID* providerKey)
          : _name{name}, _description{description}, _sublayer{}
      {
        GenerateUUID(_sublayer.subLayerKey);
        _sublayer.providerKey = providerKey;
        _sublayer.displayData.name = _name.data();
        _sublayer.displayData.description = _description.data();
        _sublayer.weight = 0xffff;
      }

      GUID& subLayerKey{_sublayer.subLayerKey};

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
          GUID key,
          std::vector<FWPM_FILTER_CONDITION0_> conditions,
          uint8_t weight,
          std::wstring name,
          std::wstring description)
          : _name{name}
          , _description{description}
          , _conditions{conditions}
          , _filter{}
          , _engine{engineHandle}
          , _ID{}
      {
        _filter.action.type = action;
        _filter.layerKey = key;
        if (sublayer)
        {
          _filter.subLayerKey = sublayer->subLayerKey;
        }
        else
          throw std::invalid_argument{"no sublayer provided"};

        if (weight)
        {
          _filter.weight.uint8 = weight;
          _filter.weight.type = FWP_UINT8;
        }
        else
          _filter.weight.type = FWP_EMPTY;

        _filter.numFilterConditions = _conditions.size();
        _filter.filterCondition = _conditions.data();

        _filter.displayData.name = _name.data();
        _filter.displayData.description = _description.data();

        if (auto err = FwpmFilterAdd0(engineHandle, &_filter, nullptr, &_ID); err != ERROR_SUCCESS)
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

    class Firewall
    {
      SubLayer _sublayer;
      HANDLE m_Handle;

      std::unordered_map<uint64_t, std::unique_ptr<Filter>> m_Filters;
      std::unordered_multimap<SockAddr, uint64_t> m_Holes;

     public:
      Firewall(HANDLE handle)
          : _sublayer{L"Lokinet Filters", L"RoutePoker Filters", nullptr}, m_Handle{handle}
      {
        if (auto err = FwpmSubLayerAdd0(m_Handle, _sublayer, 0); err != ERROR_SUCCESS)
        {
          throw win32::error{err, "fwpmSubLayerAdd0 failed: "};
        }
      }

      ~Firewall()
      {
        m_Filters.clear();
        FwpmSubLayerDeleteByKey0(m_Handle, &_sublayer.subLayerKey);
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
        const auto guid = oxenmq::to_hex(
            std::string_view{reinterpret_cast<const char*>(&layerKey), sizeof(layerKey)});
        LogInfo("adding filter ", to_width<std::string>(name), "guid=", guid);
        auto filter = std::make_unique<Filter>(
            m_Handle, _sublayer, action, layerKey, conditions, weight, name, description);

        const auto id = filter->ID();
        m_Filters[id] = std::move(filter);
        return id;
      }
    };

    std::unique_ptr<Firewall> m_Firewall;
    std::vector<NET_LUID> m_Interfaces;

    void
    PermitLoopback()
    {
      FWPM_FILTER_CONDITION0_ condition = win32::MakeCondition(
          win32::FWPM_CONDITION_FLAGS(),
          FWP_MATCH_FLAGS_ALL_SET,
          huint32_t{FWP_CONDITION_FLAG_IS_LOOPBACK});

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
    DropV6()
    {
      m_Firewall->AddFilter(
          FWP_ACTION_BLOCK,
          win32::FWPM_LAYER_ALE_AUTH_CONNECT_V6(),
          0,
          {},
          L"Drop outbound IPV6 Traffic",
          L"");

      m_Firewall->AddFilter(
          FWP_ACTION_BLOCK,
          win32::FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6(),
          0,
          {},
          L"Drop inbound IPV6 Traffic",
          L"");
    }

   protected:
    void
    AddInterface(NET_LUID uid)
    {
      m_Interfaces.emplace_back(std::move(uid));
    }

    class Session
    {
      std::wstring _name;
      std::wstring _description;
      FWPM_SESSION0 _session;

     public:
      explicit Session(const wchar_t* name, const wchar_t* description)
          : _name{name}, _description{description}, _session{}
      {
        _session.displayData.name = _name.data();
        _session.displayData.description = _description.data();
      }

      operator const FWPM_SESSION0*() const
      {
        return &_session;
      }
    };

    Session m_Session;

   public:
    FWPMRouteManager() : m_Session{L"lokinet route manager", L"manager of lokinet route poking"}
    {
      if (auto result = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, m_Session, &m_Handle);
          result != ERROR_SUCCESS)
      {
        throw win32::error{result, "cannot open fwpm engine: "};
      }
    }

    ~FWPMRouteManager()
    {
      FwpmEngineClose0(m_Handle);
    }

    void
    AddBlackhole() override
    {
      m_Firewall = std::make_unique<Firewall>(m_Handle);
      RunTransaction([this]() { DropV6(); });
    }

    void
    DelBlackhole() override
    {
      m_Firewall.reset();
    }

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
    AddRouteViaInterface(NetworkInterface& iface, IPRange range) override
    {
      const auto netif_str = dynamic_cast<const WintunInterface&>(iface).InterfaceAddressString();
      wintun::RouteExec(
          "ADD " + range.BaseAddressString() + " MASK "
          + netmask_ipv4_bits(range.HostmaskBits()).ToString() + " " + netif_str + " METRIC 2");
    }

    void
    DelRouteViaInterface(NetworkInterface& iface, IPRange range) override
    {
      const auto netif_str = dynamic_cast<const WintunInterface&>(iface).InterfaceAddressString();
      wintun::RouteExec(
          "DELETE " + range.BaseAddressString() + " MASK "
          + netmask_ipv4_bits(range.HostmaskBits()).ToString() + " " + netif_str + " METRIC 2");
    }
  };

  class Win32Platform : public wintun::API, public Platform, public FWPMRouteManager
  {
   public:
    Win32Platform() : API{}, Platform{}, FWPMRouteManager{}
    {
      RemoveAllAdapters();
    }

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override
    {
      auto adapter = std::make_shared<WintunInterface>(this, info, router);
      adapter->Start();
      AddInterface(adapter->GetUID());
      return adapter;
    };

    IRouteManager&
    RouteManager() override
    {
      return *this;
    }
  };

}  // namespace llarp::vpn

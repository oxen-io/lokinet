#pragma once

#define _Post_maybenull_

#include <wintun.h>
#include <iphlpapi.h>
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
    template <typename T>
    struct win32_deleter
    {
      void
      operator()(T* ptr) const
      {
        if (ptr)
          HeapFree(GetCurrentProcess(), 0, ptr);
      }
    };

    /// @brief bullshit replacement for std::make_unique because win32 is special
    template <typename T>
    [[nodiscard]] auto
    MakeUniquePtr()
    {
      return std::unique_ptr<T, win32_deleter<T>>(
          static_cast<T*>(HeapAlloc(GetProcessHeap(), 0, sizeof(T))));
    }

    template <typename Visit>
    void
    ForEachWIN32Interface(Visit visit)
    {
      auto table = MakeUniquePtr<MIB_IPFORWARDTABLE>();
      DWORD dwSize{};
      DWORD dwRetVal{};

      if (GetIpForwardTable(table.get(), &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
      {
        // resset pointer because windows or whatever
        table = MakeUniquePtr<MIB_IPFORWARDTABLE>();
        if (table.get() == nullptr)
          throw llarp::win32::last_error{};
      }

      if ((dwRetVal = GetIpForwardTable(table.get(), &dwSize, 0)) == NO_ERROR)
      {
        for (int i = 0; i < (int)table->dwNumEntries; i++)
        {
          visit(&table->table[i]);
        }
      }
    }
  }  // namespace

  namespace wintun
  {

    template <typename T, typename Closer>
    struct Deleter
    {
      Closer _close;
      explicit Deleter(Closer func) : _close{func}
      {}

      void
      operator()(T* ptr) const
      {
        if (ptr)
          _close(ptr, false, nullptr);
      }
    };

    using Adapter_ptr = std::unique_ptr<_WINTUN_ADAPTER, WINTUN_FREE_ADAPTER_FUNC>;
    using Session_ptr = std::unique_ptr<_TUN_SESSION, WINTUN_END_SESSION_FUNC>;

    struct API
    {
      WINTUN_OPEN_ADAPTER_FUNC _openAdapter;
      WINTUN_DELETE_ADAPTER_FUNC _deleteAdapter;
      WINTUN_FREE_ADAPTER_FUNC _freeAdapter;

      WINTUN_START_SESSION_FUNC _startSession;
      WINTUN_END_SESSION_FUNC _endSession;

      explicit API(const char* wintunlib)
      {
        auto handle = ::LoadLibrary(wintunlib);
        if (handle == nullptr)
          throw llarp::win32::last_error{"failed to open library " + std::string{wintunlib} + ": "};

        const std::map<std::string, FARPROC*> funcs{
            {"WintunOpenAdapter", (FARPROC*)&_openAdapter},
            {"WintunDeleteAdapter", (FARPROC*)&_deleteAdapter},
            {"WintunFreeAdapter", (FARPROC*)&_freeAdapter},
            {"WintunStartSession", (FARPROC*)&_startSession},
            {"WintunEndSession", (FARPROC*)&_endSession}};

        for (auto& [procname, ptr] : funcs)
        {
          LogInfo("wintun loading ", procname);
          if (FARPROC funcptr = GetProcAddress(handle, procname.c_str()))
            *ptr = funcptr;
          else
            throw llarp::win32::last_error{"could not find function " + procname + ": "};
        }
      }

      [[nodiscard]] auto
      MakeAdapterPtr(const InterfaceInfo& info) const
      {
        const auto name = llarp::to_width<std::wstring>(info.ifname);
        if (auto ptr = _openAdapter(L"Lokinet", name.c_str()))
          return Adapter_ptr{ptr, _freeAdapter};
        throw llarp::win32::last_error{"could not open adapter: "};
      }

      [[nodiscard]] auto
      MakeSessionPtr(const Adapter_ptr& adapter) const
      {
        if (auto ptr = _startSession(adapter.get(), WINTUN_MAX_RING_CAPACITY))
          return Session_ptr{ptr, _endSession};
        throw llarp::win32::last_error{"could not open session: "};
      }
    };
  }  // namespace wintun

  class WintunInterface : public NetworkInterface
  {
    wintun::API* const m_API;
    const wintun::Adapter_ptr m_Adapter;
    const wintun::Session_ptr m_Session;
    const InterfaceInfo m_Info;
    thread::Queue<net::IPPacket> m_UserToNetwork;
    AbstractRouter* const _router;

   public:
    explicit WintunInterface(
        wintun::API* api,
        const InterfaceInfo& info,
        AbstractRouter* router,
        size_t queueSize = 1024)
        : m_API{api}
        , m_Adapter{api->MakeAdapterPtr(info)}
        , m_Session{api->MakeSessionPtr(m_Adapter)}
        , m_Info{info}
        , m_UserToNetwork{queueSize}
        , _router{router}
    {}

    ~WintunInterface()
    {}

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
      if (m_UserToNetwork.empty())
        return net::IPPacket{};

      return m_UserToNetwork.popFront();
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {}
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

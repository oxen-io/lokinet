#pragma once

#include <unordered_map>
#include <llarp/net/net.hpp>
#include <llarp/ev/ev_libuv.hpp>
#include <oxenc/variant.h>

namespace mocks
{
  class Network;

  class MockUDPHandle : public llarp::UDPHandle
  {
    Network* const _net;
    std::optional<llarp::SockAddr> _addr;

   public:
    MockUDPHandle(Network* net, llarp::UDPHandle::ReceiveFunc recv)
        : llarp::UDPHandle{recv}, _net{net}
    {}

    std::optional<llarp::SockAddr>
    LocalAddr() const override
    {
      return _addr;
    }

    bool
    listen(const llarp::SockAddr& addr) override;

    bool
    send(const llarp::SockAddr&, const llarp_buffer_t&) override
    {
      return true;
    };

    void
    close() override{};
  };

  class Network : public llarp::net::Platform, public llarp::uv::Loop
  {
    std::unordered_multimap<std::string, llarp::IPRange> _network_interfaces;
    bool _snode;

    const Platform* const m_Default{Platform::Default_ptr()};

   public:
    Network(
        std::unordered_multimap<std::string, llarp::IPRange> network_interfaces, bool snode = true)
        : llarp::net::Platform{}
        , llarp::uv::Loop{1024}
        , _network_interfaces{std::move(network_interfaces)}
        , _snode{snode}
    {}

    void
    run() override
    {
      m_EventLoopThreadID = std::this_thread::get_id();
      m_Impl->run<uvw::Loop::Mode::ONCE>();
      m_Impl->close();
      // reset the event loop for reuse
      m_Impl = uvw::Loop::create();
    };

    llarp::RuntimeOptions
    Opts() const
    {
      return llarp::RuntimeOptions{false, false, _snode};
    }

    std::shared_ptr<llarp::UDPHandle>
    make_udp(UDPReceiveFunc recv) override
    {
      return std::make_shared<MockUDPHandle>(this, recv);
    }

    std::optional<std::string>
    GetBestNetIF(int af) const override
    {
      for (const auto& [k, range] : _network_interfaces)
        if (range.Family() == af and not range.BogonRange())
          return k;
      return std::nullopt;
    }

    std::optional<std::string>
    FindFreeTun() const override
    {
      return "mocktun0";
    }

    std::optional<llarp::SockAddr>
    GetInterfaceAddr(std::string_view ifname, int af) const override
    {
      for (const auto& [name, range] : _network_interfaces)
        if (range.Family() == af and name == ifname)
          return llarp::SockAddr{range.addr};
      return std::nullopt;
    }

    bool
    HasInterfaceAddress(llarp::net::ipaddr_t ip) const override
    {
      for (const auto& item : _network_interfaces)
        if (var::visit([range = item.second](auto&& ip) { return range.Contains(ToHost(ip)); }, ip))
          return true;
      // check for wildcard
      return IsWildcardAddress(ip);
    }

    std::optional<llarp::SockAddr>
    AllInterfaces(llarp::SockAddr fallback) const override
    {
      return m_Default->AllInterfaces(fallback);
    }

    llarp::SockAddr
    Wildcard(int af) const override
    {
      return m_Default->Wildcard(af);
    }

    bool
    IsBogon(const llarp::SockAddr& addr) const override
    {
      return m_Default->IsBogon(addr);
    }
    bool
    IsLoopbackAddress(llarp::net::ipaddr_t ip) const override
    {
      return m_Default->IsLoopbackAddress(ip);
    }

    bool
    IsWildcardAddress(llarp::net::ipaddr_t ip) const override
    {
      return m_Default->IsWildcardAddress(ip);
    }

    std::optional<int>
    GetInterfaceIndex(llarp::net::ipaddr_t ip) const override
    {
      return m_Default->GetInterfaceIndex(ip);
    }

    std::optional<llarp::IPRange>
    FindFreeRange() const override
    {
      auto ownsRange = [this](const auto& range) {
        for (const auto& [name, ownRange] : _network_interfaces)
        {
          if (ownRange * range)
            return true;
        }
        return false;
      };
      using namespace llarp;
      // generate possible ranges to in order of attempts
      std::list<IPRange> possibleRanges;
      for (byte_t oct = 16; oct < 32; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(172, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(10, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(192, 168, oct, 1, 24));
      }
      // for each possible range pick the first one we don't own
      for (const auto& range : possibleRanges)
      {
        if (not ownsRange(range))
          return range;
      }
      return std::nullopt;
    }

    std::string
    LoopbackInterfaceName() const override
    {
      for (const auto& [name, range] : _network_interfaces)
        if (IsLoopbackAddress(ToNet(range.addr)))
          return name;
      throw std::runtime_error{"no loopback interface?"};
    }
  };

  bool
  MockUDPHandle::listen(const llarp::SockAddr& addr)
  {
    if (not _net->HasInterfaceAddress(addr.getIP()))
      return false;
    _addr = addr;
    return true;
  }

}  // namespace mocks

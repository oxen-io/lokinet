#pragma once

#include <cstdint>
#include <llarp/dns/server.hpp>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <llarp/layers/flow/flow_addr.hpp>
#include <llarp/dns/question.hpp>
#include <llarp/dns/rr.hpp>

namespace llarp::layers::flow
{
  class FlowLayer;
}

namespace llarp::layers::platform
{

  class PlatformLayer;
  class AddrMapper;

  /// synthesizes authoritative RR for a .loki or .snode address.
  class DNSZone
  {
    /// the flow address of the dns zone this is responsible for.
    flow::FlowAddr _flow_addr;

    /// any known cached ons names for this zone.
    std::vector<std::string> _ons_names;
    std::vector<dns::SRVData> _srv;
    AddrMapper* const _addr_mapper;

    /// synth srv records rdata.
    std::vector<dns::RData>
    srv_records() const;

    /// synth cname records rdata.
    std::vector<dns::RData>
    cname_records() const;

    /// get ttl for a rr type.
    uint32_t
    ttl(dns::RRType rr_type) const;

    std::string
    zone_name() const;

    constexpr AddrMapper&
    addr_mapper() const
    {
      return *_addr_mapper;
    }

   public:
    DNSZone();
    DNSZone(AddrMapper& mapper, flow::FlowAddr addr);

    /// add a new srv recrod to this dns zone.
    void
    add_srv_record(dns::SRVTuple record);

    /// synthesize any resource records for a rr type.
    std::vector<dns::ResourceRecord>
    synth_rr_for(dns::RRType r_type) const;
  };

  /// handles dns queries, for .loki / .snode
  class DNSQueryHandler : public dns::Resolver_Base
  {
    PlatformLayer& _plat;

    /// all remote zones.
    std::unordered_map<flow::FlowAddr, DNSZone> _zones;

    /// async resolve dns zone given a dns question.
    void
    async_obtain_dns_zone(
        const dns::Question& qestion, std::function<void(std::optional<DNSZone>)> result_handler);

   public:
    explicit DNSQueryHandler(PlatformLayer& parent);
    ~DNSQueryHandler() override = default;

    int
    Rank() const override;

    std::string_view
    ResolverName() const override;

    bool
    MaybeHookDNS(
        std::shared_ptr<dns::PacketSource_Base> source,
        const dns::Message& query,
        const SockAddr& to,
        const SockAddr& from) override;
  };
}  // namespace llarp::layers::platform

#include "dns_bridge.hpp"
#include "llarp/dns/rr.hpp"
#include "oxen/log.hpp"
#include "platform_layer.hpp"
#include "addr_mapper.hpp"

#include <cstdint>
#include <llarp/dns/question.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <llarp/layers/flow/flow_layer.hpp>
#include <llarp/dns/name.hpp>
#include <llarp/util/logging.hpp>

namespace llarp::layers::platform
{

  static auto logcat = log::Cat("dns");

  DNSZone::DNSZone(const AddrMapper& addr_mapper, flow::FlowAddr addr)
      : _flow_addr{std::move(addr)}, _addr_mapper{&addr_mapper}
  {}

  void
  DNSZone::add_srv_record(dns::SRVTuple tup)
  {
    _srv.emplace_back(std::move(tup));
  }

  std::vector<dns::RData>
  DNSZone::cname_records() const
  {
    std::vector<dns::RData> datas;
    for (const auto& name : _ons_names)
      datas.emplace_back(dns::encode_dns_name(name));
    return datas;
  }

  std::vector<dns::RData>
  DNSZone::srv_records() const
  {
    std::vector<dns::RData> datas;
    for (const auto& srv : _srv)
      datas.emplace_back(srv.encode_dns(zone_name()));
    return datas;
  }

  std::string
  DNSZone::zone_name() const
  {
    return _flow_addr.ToString();
  }

  static auto
  synth_rr_from_mapping(const DNSZone& zone, const AddressMapping& mapping, dns::RRType rr_type)
  {
    std::vector<dns::RData> recs;
    if (auto maybe_v4 = mapping.dst.as_ipv4addr(); maybe_v4 and rr_type == dns::RRType::A)
      recs.emplace_back(*maybe_v4);

    else if (rr_type == dns::RRType::AAAA)
      recs.emplace_back(mapping.dst.ip);
    else if (rr_type == dns::RRType::SRV)
    {
      for (auto srv : zone.srv_records())
        recs.insert(recs.end(), std::move(srv));
    }
    else if (rr_type == dns::RRType::CNAME)
    {
      for (auto cname : zone.cname_records())
        recs.insert(recs.end(), std::move(cname));
    }
    else if (rr_type == dns::RRType::MX)
      recs.emplace_back(uint16_t{10}, dns::split_dns_name(zone.zone_name()));
  }

  std::vector<dns::ResourceRecord>
  DNSZone::synth_rr_for(dns::RRType rr_type) const
  {
    std::vector<dns::RData> recs;
    for (const auto& mapping : addr_mapper().mappings_to(_flow_addr))
    {
      if (auto maybe_v4 = mapping.dst.as_ipv4addr(); maybe_v4 and rr_type == dns::RRType::A)
        recs.emplace_back(*maybe_v4);

      else if (rr_type == dns::RRType::AAAA)
        recs.emplace_back(mapping.dst.ip);
      else if (rr_type == dns::RRType::SRV and not _srv.empty())
      {
        for (auto srv : srv_records())
          recs.insert(recs.end(), std::move(srv));
      }
      else if (rr_type == dns::RRType::CNAME and not _ons_names.empty())
      {
        for (auto cname : cname_records())
          recs.insert(recs.end(), std::move(cname));
      }
      else if (rr_type == dns::RRType::MX)
        recs.emplace_back(uint16_t{10}, dns::split_dns_name(zone_name()));
    }
    std::vector<dns::ResourceRecord> ret;
    for (auto& rdata : recs)
      ret.emplace_back(zone_name(), rr_type, std::move(rdata), ttl(rr_type));

    return ret;
  }

  uint32_t
  DNSZone::ttl(dns::RRType) const
  {
    // todo: this is a placeholder value.
    return 1;
  }

  DNSQueryHandler::DNSQueryHandler(PlatformLayer& plat) : _plat{plat}
  {}

  void
  DNSQueryHandler::async_obtain_dns_zone(
      const dns::Question& question, std::function<void(std::optional<DNSZone>)> result_handler)
  {
    if (question.IsLocalhost())
    {
      result_handler(_plat.local_dns_zone());
      return;
    }
    // todo: get auth code from config.
    std::string auth = "";
    _plat.map_remote(
        question.Domain(),
        auth,
        {},
        std::nullopt,
        [result_handler = std::move(result_handler), this](auto maybe_flow_info, auto) {
          if (not maybe_flow_info)
          {
            result_handler(std::nullopt);
            return;
          }
          const auto& addr = maybe_flow_info->dst;
          auto [itr, inserted] = _zones.try_emplace(addr, _plat.addr_mapper, addr);
          if (inserted)
            log::debug(logcat, "allocated new address for {}", addr);
          result_handler(itr->second);
        });
  }

  int
  DNSQueryHandler::Rank() const
  {
    return 1;
  }

  std::string_view
  DNSQueryHandler::ResolverName() const
  {
    return "lokinet";
  }

  bool
  DNSQueryHandler::MaybeHookDNS(
      std::shared_ptr<dns::PacketSource_Base> source,
      const dns::Message& query,
      const SockAddr& to,
      const SockAddr& from)
  {
    log::debug(logcat, "maybe handle dns query: {}");
    if (query.questions.empty())
      return false;

    // make sure this query is meant for us.
    const auto& question = query.questions.at(0);
    if (not dns::is_lokinet_tld(question.tld()))
    {
      log::trace(logcat, "question is not for lokinet tld: {}", question);
      return false;

    }  // make sure we understand the rr type requested.
    auto maybe_qtype = dns::get_rr_type(question.qtype);
    if (not maybe_qtype)
    {
      log::trace(logcat, "question RR of type {} not known", int{question.qtype});
      dns::Message reply{query};
      // no such rr type know by us. we dont serv fail so queries dont leak.
      source->SendTo(from, to, reply.nx().ToBuffer());
      return true;
    }

    async_obtain_dns_zone(
        question,
        [source, qtype = *maybe_qtype, query = dns::Message{query}, from, to](auto maybe_zone) {
          dns::Message msg{query};
          if (not maybe_zone)
          {
            log::trace(logcat, "no zone found for {}", query);
            source->SendTo(from, to, msg.nx().ToBuffer());
            return;
          }
          for (auto found_rr : maybe_zone->synth_rr_for(qtype))
            msg.answers.emplace_back(std::move(found_rr));
          log::trace(logcat, "zone found answers=({})", fmt::join(msg.answers, ","));
          source->SendTo(from, to, msg.ToBuffer());
        });
    return true;
  }

}  // namespace llarp::layers::platform

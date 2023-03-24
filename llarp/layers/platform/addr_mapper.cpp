#include "addr_mapper.hpp"
#include <fmt/core.h>
#include <algorithm>
#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <vector>
#include <llarp/layers/flow/flow_layer.hpp>
#include "llarp/layers/flow/flow_addr.hpp"
#include "llarp/layers/flow/flow_info.hpp"
#include "llarp/layers/platform/platform_addr.hpp"
#include "llarp/layers/platform/platform_layer.hpp"
#include "llarp/net/ip_range.hpp"
#include "llarp/net/net_int.hpp"

namespace llarp::layers::platform
{

  AddressMapping&
  AddressMappingEntry::access()
  {
    _last_used_at = decltype(_last_used_at)::clock::now();
    return _entry;
  }

  const AddressMapping&
  AddressMappingEntry::view() const
  {
    return _entry;
  }

  bool
  AddressMappingEntry::has_addr(const PlatformAddr& src, const PlatformAddr& dst) const
  {
    return _entry.src == src and _entry.dst == dst;
  }

  std::optional<AddressMapping>
  AddrMapper::get_addr_for_flow(const flow::FlowInfo& flow_info)
  {
    auto itr =
        find_if([&flow_info](const auto& mapping) { return mapping.flow_info == flow_info; });
    if (itr == std::end(_addrs))
      return std::nullopt;
    return itr->access();
  }

  bool
  AddressMapping::owns_range(const IPRange& range) const
  {
    return std::find(owned_ranges.begin(), owned_ranges.end(), range) != std::end(owned_ranges);
  }

  std::string
  AddressMapping::ToString() const
  {
    std::optional<std::string> flow_info_str;
    if (flow_info)
      flow_info_str = flow_info->ToString();

    return fmt::format(
        "[AddressMapping flow={} src={} dst={} owned_ranges=({})]",
        flow_info_str.value_or("none"),
        src,
        dst,
        fmt::join(owned_ranges, ", "));
  }

  AddrMapper::AddrMapper(const IPRange& range) : _our_range{range}
  {}

  AddressMapping&
  AddrMapper::allocate_mapping(std::optional<PlatformAddr> src)
  {
    auto dst = next_addr();
    auto& mapping = _addrs.emplace_back().access();
    mapping.dst = dst;
    mapping.src = src.value_or(PlatformAddr{_our_range.addr});
    return mapping;
  }

  PlatformAddr
  AddrMapper::next_addr()
  {
    const PlatformAddr highest{_our_range.HighestAddr()};
    const PlatformAddr lowest{huint128_t{1} + _our_range.addr};

    // first try using highest mapped value.
    PlatformAddr next{lowest};
    for (const auto& addr : _addrs)
      next = std::max(next, addr.view().dst);

    // we already allocated the highest one before.
    if (next == highest)
    {
      // try finding the lowest one.
      for (const auto& addr : _addrs)
        next = std::min(next, addr.view().dst);

      // we are full.
      if (next == lowest)
        throw std::runtime_error{"addr mapper full"};
      // use 1 below the lowest one we have allocated now.
      next = PlatformAddr{net::ToHost(next.ip) - huint128_t{1}};
    }

    return next;
  }

  bool
  AddrMapper::is_full() const
  {
    const PlatformAddr highest{_our_range.HighestAddr()};
    const PlatformAddr lowest{huint128_t{1} + _our_range.addr};

    PlatformAddr lo{highest}, hi{lowest};
    for (const auto& addr : _addrs)
    {
      lo = std::min(addr.view().dst, lo);
      hi = std::max(addr.view().dst, hi);
    }

    return highest == hi and lowest == lo;
  }

  std::optional<AddressMapping>
  AddrMapper::mapping_for(const PlatformAddr& src, const PlatformAddr& dst)
  {
    for (auto& ent : _addrs)
    {
      if (ent.has_addr(src, dst))
        return ent.access();
    }
    return std::nullopt;
  }

  std::vector<AddressMapping>
  AddrMapper::all_exits() const
  {
    std::vector<AddressMapping> exits;
    view_all_entries([&exits](const auto& ent) {
      if (not ent.owned_ranges.empty())
        exits.emplace_back(ent);
      return true;
    });
    return exits;
  }

  net::IPRangeMap<flow::FlowAddr>
  AddrMapper::exit_map() const
  {
    net::IPRangeMap<flow::FlowAddr> map{};
    view_all_entries([&map](const auto& ent) {
      for (const auto& range : ent.owned_ranges)
      {
        if (ent.flow_info)
          map.Insert(range, ent.flow_info->dst);
      }

      return true;
    });
    return map;
  }

  void
  AddrMapper::remove_lru()
  {
    std::chrono::steady_clock::time_point lru_time{std::chrono::steady_clock::time_point::max()};
    auto lru_itr = _addrs.end();
    for (auto itr = _addrs.begin(); itr != _addrs.end(); ++itr)
    {
      if (lru_time >= itr->last_used_at())
        continue;
      lru_itr = itr;
      lru_time = lru_itr->last_used_at();
    }
    if (lru_itr != _addrs.end())
      _addrs.erase(lru_itr);
  }

  void
  AddrMapper::put(AddressMapping&& ent)
  {
    auto itr = find_if([&ent](const auto& other) {
      if (ent.flow_info and other.flow_info)
        return *ent.flow_info == *other.flow_info;
      return ent.dst == other.dst and ent.src == other.src;
    });

    // construct it if we dont have it.
    if (itr == std::end(_addrs))
    {
      if (is_full())
        remove_lru();
      itr = _addrs.emplace(std::end(_addrs));
    }
    // access the entry for assignment to mark as just used now.
    itr->access() = ent;
  }

  void
  AddrMapper::prune(flow::FlowLayer& flow_layer)
  {
    // todo: put constexpr elsewhere.
    constexpr auto max_idle_threshold = 15min;

    remove_if_entry([&flow_layer, max_idle_threshold](const auto& ent) {
      // remove stale entries or ones that we dont have a flow on.
      const auto& maybe_flow_info = ent.view().flow_info;
      if ((not maybe_flow_info) or not flow_layer.has_flow(*maybe_flow_info))
        return ent.idle_for() > max_idle_threshold;

      // remove stale entries we have a flow for and remove the flow for them too.
      if (ent.idle_for() > max_idle_threshold)
        flow_layer.remove_flow(*maybe_flow_info);

      return not flow_layer.has_flow(*maybe_flow_info);
    });
  }

  std::vector<AddressMapping>
  AddrMapper::mappings_to(const flow::FlowAddr& remote) const
  {
    std::vector<AddressMapping> mappings;
    view_all_entries([&mappings, remote](const auto& entry) {
      if (not entry.flow_info)
        return true;
      if (entry.flow_info->dst == remote)
        mappings.emplace_back(entry);
      return true;
    });
    return mappings;
  }

}  // namespace llarp::layers::platform

namespace llarp
{
  nlohmann::json
  to_json(const layers::platform::AddressMapping& mapping)
  {
    auto ranges = nlohmann::json::array();
    for (const auto& range : mapping.owned_ranges)
      ranges.push_back(range.ToString());

    auto object = nlohmann::json::object(
        {{"ranges", ranges},
         {"src_ip",
          var::visit([](auto&& addr) { return addr.ToString(); }, mapping.src.as_ipaddr())},
         {"dst_ip",
          var::visit([](auto&& addr) { return addr.ToString(); }, mapping.dst.as_ipaddr())}});
    if (mapping.flow_info)
    {
      object["remote_addr"] = mapping.flow_info->dst.ToString();
      object["local_addr"] = mapping.flow_info->src.ToString();
    }
    return object;
  }
}  // namespace llarp

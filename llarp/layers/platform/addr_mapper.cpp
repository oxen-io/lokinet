#include "addr_mapper.hpp"
#include <bits/types/clock_t.h>
#include <fmt/core.h>
#include <algorithm>
#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <vector>
#include <llarp/layers/flow/flow_layer.hpp>
#include "llarp/layers/flow/flow_info.hpp"
#include "llarp/layers/platform/platform_addr.hpp"

namespace llarp::layers::platform
{

  static auto logcat = log::Cat("platform-layer");

  bool
  AddressMapping::operator==(const AddressMapping& other) const
  {
    return src == other.src and dst == other.dst and flow_info == other.flow_info;
  }

  AddressMapping&
  AddressMappingEntry::access()
  {
    _last_used_at = clock_t::now();
    return _entry;
  }

  const AddressMapping&
  AddressMappingEntry::view() const noexcept
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
  AddressMapping::allows_ingres(const PlatformAddr& _src, const PlatformAddr& _dst) const
  {
    if (src != _src)
      return false;

    for (const auto& range : owned_ranges)
    {
      if (range.Contains(_dst.ip))
        return true;
    }
    return dst == _dst;
  }

  bool
  AddressMapping::allows_egres(const PlatformAddr& _src, const PlatformAddr& _dst) const
  {
    return allows_ingres(_dst, _src);
  }

  bool
  AddressMapping::owns_range(const IPRange& range) const
  {
    return std::find(owned_ranges.begin(), owned_ranges.end(), range) != std::end(owned_ranges);
  }

  bool
  AddressMappingEntry::routes_addr(const PlatformAddr& dst) const
  {
    for (const auto& range : _entry.owned_ranges)
    {
      log::trace(logcat, "check range {} owns {}", range, dst);
      if (range.Contains(dst.ip))
        return true;
    }
    return false;
  }

  std::string
  AddressMapping::ToString() const
  {
    return fmt::format(
        "[AddressMapping flow={} src={} dst={} owned_ranges=({})]",
        flow_info,
        src,
        dst,
        fmt::join(owned_ranges, ", "));
  }

  AddrMapper::AddrMapper(const IPRange& range_) : range{range_}
  {}

  AddressMapping&
  AddrMapper::allocate_mapping(std::optional<PlatformAddr> src)
  {
    auto dst = next_addr();
    auto& mapping = _addrs.emplace_back().access();
    mapping.dst = dst;
    mapping.src = src.value_or(PlatformAddr{range.addr});
    return mapping;
  }

  PlatformAddr
  AddrMapper::next_addr()
  {
    const PlatformAddr highest{range.HighestAddr()};
    const PlatformAddr lowest{huint128_t{1} + range.addr};

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
  AddrMapper::is_full() const noexcept
  {
    const PlatformAddr highest{range.HighestAddr()};
    const PlatformAddr lowest{huint128_t{1} + range.addr};

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
      log::trace(logcat, "check {}", ent.view());
      if (ent.has_addr(src, dst) or ent.routes_addr(dst))
        return ent.access();
    }
    return std::nullopt;
  }

  std::vector<AddressMapping>
  AddrMapper::all_exits() const
  {
    std::vector<AddressMapping> exits;
    view_all_mappings([&exits](const auto& m) {
      if (m.is_exit())
        exits.emplace_back(m);
      return true;
    });
    return exits;
  }

  net::IPRangeMap<flow::FlowAddr>
  AddrMapper::exit_map() const
  {
    net::IPRangeMap<flow::FlowAddr> map{};
    view_all_mappings([&map](const auto& m) {
      for (const auto& range : m.owned_ranges)

        map.Insert(range, m.flow_info.dst);

      return true;
    });
    return map;
  }

  void
  AddrMapper::remove_lru()
  {
    // we dont need do anything to an empty addr mapper.
    if (_addrs.empty())
      return;
    using clock_t = std::chrono::steady_clock;
    clock_t::time_point lru_time = clock_t::time_point::max();
    auto lru_itr = _addrs.begin();
    for (auto itr = lru_itr; itr != _addrs.end(); ++itr)
    {
      const auto& last_used_at = itr->last_used_at();
      if (lru_time <= last_used_at)
        continue;
      lru_itr = itr;
      lru_time = last_used_at;
    }
    _addrs.erase(lru_itr);
  }

  AddressMapping&
  AddrMapper::put(AddressMapping&& ent)
  {
    auto itr = find_if([&ent](const auto& other) { return ent == other; });

    // construct it if we dont have it.
    if (itr == std::end(_addrs))
    {
      if (is_full())
        remove_lru();
      itr = _addrs.emplace(std::end(_addrs));
    }
    // access the entry for assignment to mark as just used now.
    auto& mapping = itr->access();
    // assign it
    mapping = ent;
    // return what we assigned.
    return mapping;
  }

  void
  AddrMapper::prune(flow::FlowLayer& flow_layer)
  {
    // todo: put constexpr elsewhere.
    constexpr auto max_idle_threshold = 15min;

    remove_if_entry([&flow_layer, max_idle_threshold](const auto& ent) {
      // remove stale entries or ones that we dont have a flow on.
      const auto& flow_info = ent.view().flow_info;
      if (not flow_layer.has_flow(flow_info))
        return ent.idle_for() > max_idle_threshold;

      // remove stale entries we have a flow for and remove the flow for them too.
      if (ent.idle_for() > max_idle_threshold)
        flow_layer.remove_flow(flow_info);

      return not flow_layer.has_flow(flow_info);
    });
  }

  std::vector<AddressMapping>
  AddrMapper::mappings_to(const flow::FlowAddr& remote) const
  {
    std::vector<AddressMapping> mappings;
    view_all_mappings([&mappings, &remote](const auto& m) {
      if (m.flow_info.dst == remote)
        mappings.emplace_back(m);
      return true;
    });
    return mappings;
  }

  bool
  AddrMapper::unmap(AddressMapping mapping)
  {
    if (auto itr = find_if([mapping](const auto& ent) { return ent == mapping; });
        itr != _addrs.end())
    {
      _addrs.erase(itr);
      return true;
    }
    return false;
  }

  bool
  AddressMapping::is_exit() const
  {
    return not owned_ranges.empty();
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

    return nlohmann::json::object(
        {{"ranges", ranges},
         {"local_addr", mapping.flow_info.src.ToString()},
         {"remote_addr", mapping.flow_info.dst.ToString()},
         {"src_ip",
          var::visit([](auto&& addr) { return addr.ToString(); }, mapping.src.as_ipaddr())},
         {"dst_ip",
          var::visit([](auto&& addr) { return addr.ToString(); }, mapping.dst.as_ipaddr())}});
  }

}  // namespace llarp

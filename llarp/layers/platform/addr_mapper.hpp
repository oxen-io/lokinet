#pragma once

#include "platform_addr.hpp"
#include <llarp/layers/flow/flow_info.hpp>
#include <llarp/net/ip_range_map.hpp>

#include <chrono>
#include <string>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace llarp::layers::flow
{
  class FlowLayer;
}

namespace llarp::layers::platform
{

  /// holds all info about a platform layer to flow layer mapping.
  /// lets us keep track of which flow we are using and which kinds of ingres/egres traffic we
  /// permit.
  struct AddressMapping
  {
    /// the current flow layer info we are using for this mapping.
    flow::FlowInfo flow_info;

    /// source and destination addresses for ingres traffic
    /// we only allow ingres from src.
    /// we only route ingres bound for dst.
    PlatformAddr src, dst;

    /// additional ranges we permit ingres traffic bound to as dst and egress traffic from as src.
    std::vector<IPRange> owned_ranges;

    /// return true if we own this exact range range.
    bool
    owns_range(const IPRange& range) const;

    /// return true if we allow ingres (os to lokinet) traffic with this src and dst address
    bool
    allows_ingres(const PlatformAddr& src, const PlatformAddr& dst) const;

    /// return true if we allow egres (lokinet to os) traffic with this src and dst address
    bool
    allows_egres(const PlatformAddr& src, const PlatformAddr& dst) const;

    std::string
    ToString() const;

    /// helper that tells us if we permit more than just traffic to dst.
    bool
    is_exit() const;

    AddressMapping() = default;
    AddressMapping(const AddressMapping&) = default;
    AddressMapping(AddressMapping&&) = default;

    AddressMapping&
    operator=(AddressMapping&&) = default;
    AddressMapping&
    operator=(const AddressMapping&) = default;

    bool
    operator==(const AddressMapping& other) const;
  };

  /// container that holds an addressmapping and extra metadata we need in the addrmapper.
  class AddressMappingEntry
  {
    using clock_t = std::chrono::steady_clock;
    /// the time this entry was last used.
    clock_t::time_point _last_used_at;
    /// the entry itself.
    AddressMapping _entry;

   public:
    /// return true if we match this src and dst address.
    bool
    has_addr(const PlatformAddr& src, const PlatformAddr& dst) const;

    /// return true if we can route to this destination address.
    bool
    routes_addr(const PlatformAddr& dst) const;

    /// return true if this entry is for this flow info.
    bool
    has_flow_info(const flow::FlowInfo& flow_info) const;

    /// a helper that returns how long since we last used this entry.
    inline auto
    idle_for() const noexcept
    {
      return clock_t::now() - _last_used_at;
    }

    /// readonly accessor for _last_used_at.
    constexpr const auto&
    last_used_at() const
    {
      return _last_used_at;
    }

    /// update last used and access the entry.
    AddressMapping&
    access();

    /// view address mapping without updating last use.
    const AddressMapping&
    view() const noexcept;
  };

  /// in charge of mapping flow addresses and platform addresses to each other
  class AddrMapper
  {
    /// all remotely routable ranges and their metadata.
    std::vector<AddressMappingEntry> _addrs;
    /// remove the lease recently used mapping.
    void
    remove_lru();

    /// return a free address we can map to.
    /// throws if full.
    PlatformAddr
    next_addr();

    friend class ReservedAddressMapping;

   public:
    /// our ip range that we are providing to the local system we are running on.
    const IPRange range;

    /// construct addr mapper with an ip range to provide to be routable locally to the system we
    /// run on.
    explicit AddrMapper(const IPRange& range);

    /// unconditionally put an entry into the addrmapper, stomping any existing entry that used to
    /// match the destination it went to. if the address mapper is full if we had to insert a new
    /// entry, we remove the least frequenty used entry. returns a non const l-value reference to
    /// the entry that now exists in the address map.
    AddressMapping&
    put(AddressMapping&& ent);

    /// unmap an address mapping.
    /// returns true if it was removed.
    bool
    unmap(AddressMapping ent);

    /// remove all mappings we have no flows to.
    void
    prune(flow::FlowLayer&);

    /// get a platform address for a flow if it exists and update its last used time.
    /// returns std::nullopt if there is now mapping for this flow.
    std::optional<AddressMapping>
    get_addr_for_flow(const flow::FlowInfo& flow);

    /// return true if we have an address for this flow info but will not update last used time of
    /// the entry if it exists.
    bool
    has_addr_for_flow(const flow::FlowInfo& flow) const;

    /// find the mapping that matches src/dst address.
    /// updates last use time on the chosen entry.
    std::optional<AddressMapping>
    mapping_for(const PlatformAddr& src, const PlatformAddr& dst);

    /// return all address mappings to this remote and do not update last used time on any of the
    /// entries.
    std::vector<AddressMapping>
    mappings_to(const flow::FlowAddr& remote) const;

    /// return true if we have mapped as many ip addresses as we are able to.
    bool
    is_full() const noexcept;

    /// explicitly allocate a new address mapping with an unused destination platform addr.
    /// if a source address is not provided, our interface address with be used.
    /// throws if the addrmapper is full.
    AddressMapping&
    allocate_mapping(std::optional<PlatformAddr> src = std::nullopt);

    /// read only iterate over all mappings.
    /// visit returns false to break iteration.
    template <typename Viewer_t>
    void
    view_all_mappings(Viewer_t&& visit) const
    {
      for (const auto& ent : _addrs)
      {
        if (not visit(ent.view()))
          return;
      }
    }

    /// return an iterator for first mappings matching a predicate. will not update last used time.
    template <typename Predicate_t>
    [[nodiscard]] auto
    find_if(Predicate_t&& pred) noexcept
    {
      return std::find_if(
          _addrs.begin(), _addrs.end(), [pred](const auto& ent) { return pred(ent.view()); });
    }

    /// return a const iterator for first mapping matching a predicate. will not update last used
    /// time.
    template <typename Predicate_t>
    [[nodiscard]] auto
    const_find_if(Predicate_t&& pred) const noexcept
    {
      return std::find_if(
          _addrs.cbegin(), _addrs.cend(), [pred](const auto& ent) { return pred(ent.view()); });
    }

    /// remove all entries that matches a predicate.
    template <typename Predicate_t>
    void
    remove_if_entry(Predicate_t&& pred) noexcept
    {
      auto itr = _addrs.begin();
      while (itr != _addrs.end())
      {
        if (pred(*itr))
          itr = _addrs.erase(itr);
        else
          ++itr;
      }
    }

    /// remove all mappings that match a predicate.
    template <typename Predicate_t>
    void
    remove_if(Predicate_t&& pred) noexcept
    {
      _addrs.erase(const_find_if(pred));
    }

    /// return all mappings that we have that carry exit traffic.
    /// last use time on entries will not be updated.
    std::vector<AddressMapping>
    all_exits() const;

    /// make an ip range map for all our exits.
    net::IPRangeMap<flow::FlowAddr>
    exit_map() const;
  };

}  // namespace llarp::layers::platform

namespace llarp
{
  nlohmann::json
  to_json(const llarp::layers::platform::AddressMapping&);

  template <>
  inline constexpr bool IsToStringFormattable<layers::platform::AddressMapping> = true;
}  // namespace llarp

#pragma once

#include "platform_addr.hpp"
#include <llarp/layers/flow/flow_info.hpp>

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

  class AddrMapper;

  struct AddressMapping
  {
    std::optional<flow::FlowInfo> flow_info;
    PlatformAddr src, dst;
    /// ip ranges for "exit"
    std::vector<IPRange> owned_ranges;

    /// return true if we own this exact range range.
    bool
    owns_range(const IPRange& range) const;

    std::string
    ToString() const;

    bool
    is_exit() const;
  };

  /// container that holds an addressmapping and extra metadata we need in the addrmapper.
  class AddressMappingEntry
  {
    /// the time this entry was last used.
    std::chrono::steady_clock::time_point _last_used_at;
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

    /// return how long since we last used this entry.
    inline auto
    idle_for() const
    {
      return decltype(_last_used_at)::clock::now() - _last_used_at;
    }

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
    view() const;
  };

  /// in charge of mapping flow addresses and platform addresses to each other
  class AddrMapper
  {
    // remote flow mappings
    std::vector<AddressMappingEntry> _addrs;

    const IPRange _our_range;

    /// remove the lease recently used mapping.
    void
    remove_lru();

    /// return a free address we can map to.
    /// throws if full.
    PlatformAddr
    next_addr();

    friend class ReservedAddressMapping;

   public:
    explicit AddrMapper(const IPRange& range);

    const IPRange range;

    /// unconditionally put an entry in.
    /// stomps existing entry.
    /// if full, removes the least frequenty used mapping.
    void
    put(AddressMapping&&);

    /// prune all mappings we have no flows for
    void
    prune(flow::FlowLayer&);

    /// get a platform for a flow if it exists. updates last used time.
    std::optional<AddressMapping>
    get_addr_for_flow(const flow::FlowInfo& flow);

    /// return true if we have an address for this flow info, will not update last used time.
    bool
    has_addr_for_flow(const flow::FlowInfo& flow) const;

    /// find a mapping that best matches src/dst address.
    /// updates last use time.
    std::optional<AddressMapping>
    mapping_for(const PlatformAddr& src, const PlatformAddr& dst);

    /// return all address mappings to this remote. does not update last used time.
    std::vector<AddressMapping>
    mappings_to(const flow::FlowAddr& remote) const;

    /// return true if we have mapped as many ip addresses as we are able to.
    bool
    is_full() const;

    /// get a new address mapping with a filled out destination address and an optionally provided
    /// source address. if the source address is nullopt we will use the base address of the range
    /// as the source. throws if full.
    AddressMapping&
    allocate_mapping(std::optional<PlatformAddr> src = std::nullopt);

    /// read only iterate over all entries.
    /// visit returns false to break iteration.
    template <typename Viewer_t>
    void
    view_all_entries(Viewer_t&& visit) const
    {
      for (const auto& ent : _addrs)
      {
        if (not visit(ent.view()))
          return;
      }
    }

    /// return an iterator for first entry matching predicate.
    /// this is a helper so we will not update last used.
    template <typename Predicate_t>
    [[nodiscard]] auto
    find_if(Predicate_t&& pred)
    {
      return std::find_if(
          _addrs.begin(), _addrs.end(), [pred](const auto& ent) { return pred(ent.view()); });
    }

    /// return an iterator for first entry matching predicate.
    /// this is a helper so we will not update last used.
    template <typename Predicate_t>
    [[nodiscard]] auto
    find_if(Predicate_t&& pred) const
    {
      return std::find_if(
          _addrs.begin(), _addrs.end(), [pred](const auto& ent) { return pred(ent.view()); });
    }

    /// remove via predicate on entry.
    template <typename Predicate_t>
    void
    remove_if_entry(Predicate_t&& pred)
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

    /// remove via predicate on mapping.
    template <typename Predicate_t>
    void
    remove_if_mapping(Predicate_t&& pred)
    {
      remove_if_entry([pred](auto& ent) { return pred(ent.view()); });
    }

    /// return all exits we have.
    /// last use time not updated.
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

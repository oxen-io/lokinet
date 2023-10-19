#pragma once

#include "router_contact.hpp"
#include "router_id.hpp"

#include "util/common.hpp"
#include "util/fs.hpp"
#include "dht/key.hpp"
#include "crypto/crypto.hpp"
#include "util/thread/threading.hpp"
#include "util/thread/annotations.hpp"
#include <llarp/router/router.hpp>

#include <set>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <algorithm>

namespace llarp
{
  struct Router;

  class NodeDB
  {
    struct Entry
    {
      const RouterContact rc;
      llarp_time_t insertedAt;
      explicit Entry(RouterContact rc);
    };

    using NodeMap = std::unordered_map<RouterID, Entry>;

    NodeMap entries;

    const Router& router;
    const fs::path m_Root;
    const std::function<void(std::function<void()>)> disk;

    llarp_time_t m_NextFlushAt;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    remove_many_from_disk_async(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    get_path_by_pubkey(RouterID pk) const;

   public:
    explicit NodeDB(
        fs::path rootdir, std::function<void(std::function<void()>)> diskCaller, Router* r);

    /// in memory nodedb
    NodeDB();

    /// load all entries from disk syncrhonously
    void
    load_from_disk();

    /// explicit save all RCs to disk synchronously
    void
    save_to_disk() const;

    /// the number of RCs that are loaded from disk
    size_t
    num_loaded() const;

    /// do periodic tasks like flush to disk and expiration
    void
    Tick(llarp_time_t now);

    /// find the absolute closets router to a dht location
    RouterContact
    find_closest_to(dht::Key_t location) const;

    /// find many routers closest to dht key
    std::vector<RouterContact>
    find_many_closest_to(dht::Key_t location, uint32_t numRouters) const;

    /// return true if we have an rc by its ident pubkey
    bool
    has_router(RouterID pk) const;

    /// maybe get an rc by its ident pubkey
    std::optional<RouterContact>
    get_rc(RouterID pk) const;

    template <typename Filter>
    std::optional<RouterContact>
    GetRandom(Filter visit) const
    {
      return router.loop()->call_get([visit]() -> std::optional<RouterContact> {
        std::vector<const decltype(entries)::value_type*> entries;
        for (const auto& entry : entries)
          entries.push_back(entry);

        std::shuffle(entries.begin(), entries.end(), llarp::CSRNG{});

        for (const auto entry : entries)
        {
          if (visit(entry->second.rc))
            return entry->second.rc;
        }

        return std::nullopt;
      });
    }

    /// visit all entries
    template <typename Visit>
    void
    VisitAll(Visit visit) const
    {
      router.loop()->call([this, visit]() {
        for (const auto& item : entries)
          visit(item.second.rc);
      });
    }

    /// visit all entries inserted before a timestamp
    template <typename Visit>
    void
    VisitInsertedBefore(Visit visit, llarp_time_t insertedBefore)
    {
      router.loop()->call([this, visit, insertedBefore]() {
        for (const auto& item : entries)
        {
          if (item.second.insertedAt < insertedBefore)
            visit(item.second.rc);
        }
      });
    }

    /// remove an entry via its ident pubkey
    void
    remove_router(RouterID pk);

    /// remove an entry given a filter that inspects the rc
    template <typename Filter>
    void
    RemoveIf(Filter visit)
    {
      router.loop()->call([this, visit]() {
        std::unordered_set<RouterID> removed;
        auto itr = entries.begin();
        while (itr != entries.end())
        {
          if (visit(itr->second.rc))
          {
            removed.insert(itr->second.rc.pubkey);
            itr = entries.erase(itr);
          }
          else
            ++itr;
        }
        if (not removed.empty())
          remove_many_from_disk_async(std::move(removed));
      });
    }

    /// remove rcs that are not in keep and have been inserted before cutoff
    void
    remove_stale_rcs(std::unordered_set<RouterID> keep, llarp_time_t cutoff);

    /// put this rc into the cache if it is not there or newer than the one there already
    void
    put_rc_if_newer(RouterContact rc);

    /// unconditional put of rc into cache
    void
    put_rc(RouterContact rc);
  };
}  // namespace llarp

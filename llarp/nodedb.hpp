#pragma once

#include "router_contact.hpp"
#include "router_id.hpp"
#include "util/common.hpp"
#include "util/fs.hpp"
#include "util/thread/threading.hpp"
#include "util/thread/annotations.hpp"
#include "dht/key.hpp"
#include "crypto/crypto.hpp"

#include <set>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <algorithm>

namespace llarp
{
  class NodeDB
  {
    struct Entry
    {
      const RouterContact rc;
      llarp_time_t insertedAt;
      explicit Entry(RouterContact rc);
    };
    using NodeMap = std::unordered_map<RouterID, Entry>;

    NodeMap m_Entries;

    const fs::path m_Root;

    const std::function<void(std::function<void()>)> disk;

    llarp_time_t m_NextFlushAt;

    mutable util::NullMutex m_Access;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    AsyncRemoveManyFromDisk(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    GetPathForPubkey(RouterID pk) const;

   public:
    explicit NodeDB(fs::path rootdir, std::function<void(std::function<void()>)> diskCaller);

    /// in memory nodedb
    NodeDB();

    /// load all entries from disk syncrhonously
    void
    LoadFromDisk();

    /// explicit save all RCs to disk synchronously
    void
    SaveToDisk() const;

    /// the number of RCs that are loaded from disk
    size_t
    NumLoaded() const;

    /// do periodic tasks like flush to disk and expiration
    void
    Tick(llarp_time_t now);

    /// find the absolute closets router to a dht location
    RouterContact
    FindClosestTo(dht::Key_t location) const;

    /// find many routers closest to dht key
    std::vector<RouterContact>
    FindManyClosestTo(dht::Key_t location, uint32_t numRouters) const;

    /// return true if we have an rc by its ident pubkey
    bool
    Has(RouterID pk) const;

    /// maybe get an rc by its ident pubkey
    std::optional<RouterContact>
    Get(RouterID pk) const;

    template <typename Filter>
    std::optional<RouterContact>
    GetRandom(Filter visit) const
    {
      util::NullLock lock{m_Access};

      std::vector<const decltype(m_Entries)::value_type*> entries;
      for (const auto& entry : m_Entries)
        entries.push_back(&entry);

      std::shuffle(entries.begin(), entries.end(), llarp::CSRNG{});

      for (const auto entry : entries)
      {
        if (visit(entry->second.rc))
          return entry->second.rc;
      }

      return std::nullopt;
    }

    /// visit all entries
    template <typename Visit>
    void
    VisitAll(Visit visit) const
    {
      util::NullLock lock{m_Access};
      for (const auto& item : m_Entries)
      {
        visit(item.second.rc);
      }
    }

    /// visit all entries inserted before a timestamp
    template <typename Visit>
    void
    VisitInsertedBefore(Visit visit, llarp_time_t insertedBefore)
    {
      util::NullLock lock{m_Access};
      for (const auto& item : m_Entries)
      {
        if (item.second.insertedAt < insertedBefore)
          visit(item.second.rc);
      }
    }

    /// remove an entry via its ident pubkey
    void
    Remove(RouterID pk);

    /// remove an entry given a filter that inspects the rc
    template <typename Filter>
    void
    RemoveIf(Filter visit)
    {
      util::NullLock lock{m_Access};
      std::unordered_set<RouterID> removed;
      auto itr = m_Entries.begin();
      while (itr != m_Entries.end())
      {
        if (visit(itr->second.rc))
        {
          removed.insert(itr->second.rc.pubkey);
          itr = m_Entries.erase(itr);
        }
        else
          ++itr;
      }
      if (not removed.empty())
        AsyncRemoveManyFromDisk(std::move(removed));
    }

    /// remove rcs that are not in keep and have been inserted before cutoff
    void
    RemoveStaleRCs(std::unordered_set<RouterID> keep, llarp_time_t cutoff);

    /// put this rc into the cache if it is not there or newer than the one there already
    void
    PutIfNewer(RouterContact rc);

    /// unconditional put of rc into cache
    void
    Put(RouterContact rc);
  };
}  // namespace llarp

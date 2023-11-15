#include "nodedb.hpp"

#include "crypto/types.hpp"
#include "dht/kademlia.hpp"
#include "router_contact.hpp"
#include "util/time.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

static const char skiplist_subdirs[] = "0123456789abcdef";
static const std::string RC_FILE_EXT = ".signed";

namespace llarp
{
  NodeDB::Entry::Entry(RemoteRC value) : rc(std::move(value)), insertedAt(llarp::time_now_ms())
  {}

  static void
  EnsureSkiplist(fs::path nodedbDir)
  {
    if (not fs::exists(nodedbDir))
    {
      // if the old 'netdb' directory exists, move it to this one
      fs::path parent = nodedbDir.parent_path();
      fs::path old = parent / "netdb";
      if (fs::exists(old))
        fs::rename(old, nodedbDir);
      else
        fs::create_directory(nodedbDir);
    }

    if (not fs::is_directory(nodedbDir))
      throw std::runtime_error{fmt::format("nodedb {} is not a directory", nodedbDir)};

    for (const char& ch : skiplist_subdirs)
    {
      // this seems to be a problem on all targets
      // perhaps cpp17::fs is just as screwed-up
      // attempting to create a folder with no name
      // what does this mean...?
      if (!ch)
        continue;

      fs::path sub = nodedbDir / std::string(&ch, 1);
      fs::create_directory(sub);
    }
  }

  constexpr auto FlushInterval = 5min;

  NodeDB::NodeDB(fs::path root, std::function<void(std::function<void()>)> diskCaller, Router* r)
      : router{*r}
      , m_Root{std::move(root)}
      , disk(std::move(diskCaller))
      , m_NextFlushAt{time_now_ms() + FlushInterval}
  {
    EnsureSkiplist(m_Root);
  }

  void
  NodeDB::Tick(llarp_time_t now)
  {
    if (m_NextFlushAt == 0s)
      return;

    if (now > m_NextFlushAt)
    {
      router.loop()->call([this]() {
        m_NextFlushAt += FlushInterval;
        // make copy of all rcs
        std::vector<RemoteRC> copy;

        for (const auto& item : entries)
          copy.push_back(item.second.rc);

        // flush them to disk in one big job
        // TODO: split this up? idk maybe some day...
        disk([this, data = std::move(copy)]() {
          for (const auto& rc : data)
            rc.write(get_path_by_pubkey(rc.router_id()));
        });
      });
    }
  }

  fs::path
  NodeDB::get_path_by_pubkey(RouterID pubkey) const
  {
    std::string hexString = oxenc::to_hex(pubkey.begin(), pubkey.end());
    std::string skiplistDir;

    const llarp::RouterID r{pubkey};
    std::string fname = r.ToString();

    skiplistDir += hexString[0];
    fname += RC_FILE_EXT;
    return m_Root / skiplistDir / fname;
  }

  void
  NodeDB::set_bootstrap_routers(const std::set<RemoteRC>& rcs)
  {
    bootstraps.clear();  // this function really shouldn't be called more than once, but...
    for (const auto& rc : rcs)
      bootstraps.emplace(rc.router_id(), rc);
  }

  void
  NodeDB::set_router_whitelist(
      const std::vector<RouterID>& whitelist,
      const std::vector<RouterID>& greylist,
      const std::vector<RouterID>& greenlist)
  {
    if (whitelist.empty())
      return;

    registered_routers.clear();
    registered_routers.insert(whitelist.begin(), whitelist.end());
    registered_routers.insert(greylist.begin(), greylist.end());
    registered_routers.insert(greenlist.begin(), greenlist.end());

    router_whitelist.clear();
    router_whitelist.insert(whitelist.begin(), whitelist.end());
    router_greylist.clear();
    router_greylist.insert(greylist.begin(), greylist.end());
    router_greenlist.clear();
    router_greenlist.insert(greenlist.begin(), greenlist.end());

    log::info(
        logcat, "lokinet service node list now has ", router_whitelist.size(), " active routers");
  }

  std::optional<RouterID>
  NodeDB::get_random_whitelist_router() const
  {
    const auto sz = router_whitelist.size();
    if (sz == 0)
      return std::nullopt;
    auto itr = router_whitelist.begin();
    if (sz > 1)
      std::advance(itr, randint() % sz);
    return *itr;
  }

  bool
  NodeDB::is_connection_allowed(const RouterID& remote) const
  {
    if (pinned_edges.size() && pinned_edges.count(remote) == 0 && !bootstraps.count(remote))
    {
      return false;
    }

    if (not router.is_service_node())
      return true;

    return router_whitelist.count(remote) or router_greylist.count(remote);
  }

  bool
  NodeDB::is_first_hop_allowed(const RouterID& remote) const
  {
    if (pinned_edges.size() && pinned_edges.count(remote) == 0)
      return false;
    return true;
  }

  void
  NodeDB::load_from_disk()
  {
    if (m_Root.empty())
      return;

    router.loop()->call([this]() {
      std::set<fs::path> purge;

      for (const char& ch : skiplist_subdirs)
      {
        if (!ch)
          continue;
        std::string p;
        p += ch;
        fs::path sub = m_Root / p;

        llarp::util::IterDir(sub, [&](const fs::path& f) -> bool {
          // skip files that are not suffixed with .signed
          if (not(fs::is_regular_file(f) and f.extension() == RC_FILE_EXT))
            return true;

          RemoteRC rc{};

          if (not rc.read(f))
          {
            // try loading it, purge it if it is junk
            purge.emplace(f);
            return true;
          }

          if (rc.is_expired(time_now_ms()))
          {
            // rc expired dont load it and purge it later
            purge.emplace(f);
            return true;
          }

          // validate signature and purge entries with invalid signatures
          // load ones with valid signatures
          if (rc.verify())
            entries.emplace(rc.router_id(), rc);
          else
            purge.emplace(f);

          return true;
        });
      }

      if (not purge.empty())
      {
        log::warning(logcat, "removing {} invalid RCs from disk", purge.size());

        for (const auto& fpath : purge)
          fs::remove(fpath);
      }
    });
  }

  void
  NodeDB::save_to_disk() const
  {
    if (m_Root.empty())
      return;

    router.loop()->call([this]() {
      for (const auto& item : entries)
        item.second.rc.write(get_path_by_pubkey(item.first));
    });
  }

  bool
  NodeDB::has_router(RouterID pk) const
  {
    return entries.count(pk);
  }

  std::optional<RemoteRC>
  NodeDB::get_rc(RouterID pk) const
  {
    const auto itr = entries.find(pk);

    if (itr == entries.end())
      return std::nullopt;

    return itr->second.rc;
  }

  void
  NodeDB::remove_router(RouterID pk)
  {
    router.loop()->call([this, pk]() {
      entries.erase(pk);
      remove_many_from_disk_async({pk});
    });
  }

  void
  NodeDB::remove_stale_rcs(std::unordered_set<RouterID> keep, llarp_time_t cutoff)
  {
    router.loop()->call([this, keep, cutoff]() {
      std::unordered_set<RouterID> removed;
      auto itr = entries.begin();
      while (itr != entries.end())
      {
        if (itr->second.insertedAt < cutoff and keep.count(itr->second.rc.router_id()) == 0)
        {
          removed.insert(itr->second.rc.router_id());
          itr = entries.erase(itr);
        }
        else
          ++itr;
      }
      if (not removed.empty())
        remove_many_from_disk_async(std::move(removed));
    });
  }

  void
  NodeDB::put_rc(RemoteRC rc)
  {
    router.loop()->call([this, rc]() {
      const auto& rid = rc.router_id();
      entries.erase(rid);
      entries.emplace(rid, rc);
    });
  }

  size_t
  NodeDB::num_loaded() const
  {
    return router.loop()->call_get([this]() { return entries.size(); });
  }

  void
  NodeDB::put_rc_if_newer(RemoteRC rc)
  {
    router.loop()->call([this, rc]() {
      auto itr = entries.find(rc.router_id());
      if (itr == entries.end() or itr->second.rc.other_is_newer(rc))
      {
        // delete if existing
        if (itr != entries.end())
          entries.erase(itr);
        // add new entry
        entries.emplace(rc.router_id(), rc);
      }
    });
  }

  void
  NodeDB::remove_many_from_disk_async(std::unordered_set<RouterID> remove) const
  {
    if (m_Root.empty())
      return;
    // build file list
    std::set<fs::path> files;
    for (auto id : remove)
    {
      files.emplace(get_path_by_pubkey(std::move(id)));
    }
    // remove them from the disk via the diskio thread
    disk([files]() {
      for (auto fpath : files)
        fs::remove(fpath);
    });
  }

  RemoteRC
  NodeDB::find_closest_to(llarp::dht::Key_t location) const
  {
    return router.loop()->call_get([this, location]() -> RemoteRC {
      RemoteRC rc;
      const llarp::dht::XorMetric compare(location);

      VisitAll([&rc, compare](const auto& otherRC) {
        const auto& rid = rc.router_id();

        if (rid.IsZero() || compare(dht::Key_t{otherRC.router_id()}, dht::Key_t{rid}))
        {
          rc = otherRC;
          return;
        }
      });
      return rc;
    });
  }

  std::vector<RemoteRC>
  NodeDB::find_many_closest_to(llarp::dht::Key_t location, uint32_t numRouters) const
  {
    return router.loop()->call_get([this, location, numRouters]() -> std::vector<RemoteRC> {
      std::vector<const RemoteRC*> all;

      all.reserve(entries.size());
      for (auto& entry : entries)
      {
        all.push_back(&entry.second.rc);
      }

      auto it_mid = numRouters < all.size() ? all.begin() + numRouters : all.end();
      std::partial_sort(
          all.begin(), it_mid, all.end(), [compare = dht::XorMetric{location}](auto* a, auto* b) {
            return compare(*a, *b);
          });

      std::vector<RemoteRC> closest;
      closest.reserve(numRouters);
      for (auto it = all.begin(); it != it_mid; ++it)
        closest.push_back(**it);

      return closest;
    });
  }
}  // namespace llarp

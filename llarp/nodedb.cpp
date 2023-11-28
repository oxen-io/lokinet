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

        for (const auto& item : known_rcs)
          copy.push_back(item.second);

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

  bool
  NodeDB::want_rc(const RouterID& rid) const
  {
    if (not router.is_service_node())
      return true;
    return registered_routers.count(rid);
  }

  void
  NodeDB::set_bootstrap_routers(const std::set<RemoteRC>& rcs)
  {
    bootstraps.clear();  // this function really shouldn't be called more than once, but...
    for (const auto& rc : rcs)
    {
      bootstraps.emplace(rc.router_id(), rc);
    }
  }

  /// Called in normal operation when the relay we fetched RCs from gives either a "bad"
  /// response or a timeout.  Attempts to switch to a new relay as our RC source, using
  /// existing connections if possible, and respecting pinned edges.
  void
  NodeDB::rotate_rc_source()
  {
    auto conn_count = router.link_manager().get_num_connected();

    // This function makes no sense to be called if we have no connections...
    if (conn_count == 0)
      throw std::runtime_error{"Called rotate_rc_source with no connections, does not make sense!"};

    // We should not be in this function if client_known_routers isn't populated
    if (client_known_routers.size() <= 1)
      throw std::runtime_error{"Cannot rotate RC source without RC source(s) to rotate to!"};

    RemoteRC new_source{};
    router.link_manager().get_random_connected(new_source);
    if (conn_count == 1)
    {
      // if we only have one connection, it must be current rc fetch source
      assert(new_source.router_id() == rc_fetch_source);

      if (pinned_edges.size() == 1)
      {
        // only one pinned edge set, use it even though it gave unsatisfactory RCs
        assert(rc_fetch_source == *(pinned_edges.begin()));
        log::warning(
            logcat,
            "Single pinned edge {} gave bad RC response; still using it despite this.",
            rc_fetch_source);
        return;
      }

      // only one connection, choose a new relay to connect to for rc fetching

      RouterID r = rc_fetch_source;
      while (r == rc_fetch_source)
      {
        std::sample(client_known_routers.begin(), client_known_routers.end(), &r, 1, csrng);
      }
      rc_fetch_source = std::move(r);
      return;
    }

    // choose one of our other existing connections to use as the RC fetch source
    while (new_source.router_id() == rc_fetch_source)
    {
      router.link_manager().get_random_connected(new_source);
    }
    rc_fetch_source = new_source.router_id();
  }

  // TODO: trust model
  void
  NodeDB::ingest_rcs(RouterID source, std::vector<RemoteRC> rcs, rc_time timestamp)
  {
    (void)source;

    // TODO: if we don't currently have a "trusted" relay we've been fetching from,
    // this will be a full list of RCs.  We need to first check if it aligns closely
    // with our trusted RouterID list, then replace our RCs with the incoming set.

    for (auto& rc : rcs)
      put_rc_if_newer(std::move(rc), timestamp);

    // TODO: if we have a "trusted" relay we've been fetching from, this will be
    // an incremental update to the RC list, so *after* insertion we check if the
    // RCs' RouterIDs closely match our trusted RouterID list.

    last_rc_update_relay_timestamp = timestamp;
  }

  // TODO: trust model
  void
  NodeDB::ingest_router_ids(RouterID source, std::vector<RouterID> ids)
  {
    router_id_fetch_responses[source] = std::move(ids);

    router_id_response_count++;
    if (router_id_response_count == router_id_fetch_sources.size())
    {
      // TODO: reconcile all the responses, for now just insert all
      for (const auto& [rid, responses] : router_id_fetch_responses)
      {
        // TODO: empty == failure, handle that case
        for (const auto& response : responses)
        {
          client_known_routers.insert(std::move(response));
        }
      }
      router_id_fetch_in_progress = false;
    }
  }

  void
  NodeDB::fetch_rcs()
  {
    std::vector<RouterID> needed;

    const auto now =
        std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    for (const auto& [rid, rc] : known_rcs)
    {
      if (now - rc.timestamp() > RouterContact::OUTDATED_AGE)
        needed.push_back(rid);
    }

    router.link_manager().fetch_rcs(
        rc_fetch_source, last_rc_update_relay_timestamp, std::move(needed));
  }

  void
  NodeDB::fetch_router_ids()
  {
    if (router_id_fetch_in_progress)
      return;
    if (router_id_fetch_sources.empty())
      select_router_id_sources();

    // if we *still* don't have fetch sources, we can't exactly fetch...
    if (router_id_fetch_sources.empty())
    {
      log::info(logcat, "Attempting to fetch RouterIDs, but have no source from which to do so.");
      return;
    }

    router_id_fetch_in_progress = true;
    router_id_response_count = 0;
    router_id_fetch_responses.clear();
    for (const auto& rid : router_id_fetch_sources)
      router.link_manager().fetch_router_ids(rid);
  }

  void
  NodeDB::select_router_id_sources(std::unordered_set<RouterID> excluded)
  {
    // TODO: bootstrapping should be finished before this is called, so this
    //       shouldn't happen; need to make sure that's the case.
    if (client_known_routers.empty())
      return;

    // keep using any we've been using, but remove `excluded` ones
    for (const auto& r : excluded)
      router_id_fetch_sources.erase(r);

    // only know so many routers, so no need to randomize
    if (client_known_routers.size() <= (ROUTER_ID_SOURCE_COUNT + excluded.size()))
    {
      for (const auto& r : client_known_routers)
      {
        if (excluded.count(r))
          continue;
        router_id_fetch_sources.insert(r);
      }
    }

    // select at random until we have chosen enough
    while (router_id_fetch_sources.size() < ROUTER_ID_SOURCE_COUNT)
    {
      RouterID r;
      std::sample(client_known_routers.begin(), client_known_routers.end(), &r, 1, csrng);
      if (excluded.count(r) == 0)
        router_id_fetch_sources.insert(r);
    }
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
    if (pinned_edges.size() && pinned_edges.count(remote) == 0 && bootstraps.count(remote) == 0)
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

    std::set<fs::path> purge;

    const auto now = time_now_ms();

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

        if (rc.is_expired(now))
        {
          // rc expired dont load it and purge it later
          purge.emplace(f);
          return true;
        }

        known_rcs.emplace(rc.router_id(), rc);
        // TODO: the list of relays should be maintained and stored separately from
        // the RCs, as we keep older RCs around in case we go offline and need to
        // bootstrap, but they shouldn't be in the "good relays" list.
        client_known_routers.insert(rc.router_id());

        return true;
      });
    }

    if (not purge.empty())
    {
      log::warning(logcat, "removing {} invalid RCs from disk", purge.size());

      for (const auto& fpath : purge)
        fs::remove(fpath);
    }
  }

  void
  NodeDB::save_to_disk() const
  {
    if (m_Root.empty())
      return;

    router.loop()->call([this]() {
      for (const auto& item : known_rcs)
        item.second.write(get_path_by_pubkey(item.first));
    });
  }

  bool
  NodeDB::has_rc(RouterID pk) const
  {
    return known_rcs.count(pk);
  }

  std::optional<RemoteRC>
  NodeDB::get_rc(RouterID pk) const
  {
    const auto itr = known_rcs.find(pk);

    if (itr == known_rcs.end())
      return std::nullopt;

    return itr->second;
  }

  void
  NodeDB::remove_router(RouterID pk)
  {
    router.loop()->call([this, pk]() {
      known_rcs.erase(pk);
      remove_many_from_disk_async({pk});
    });
  }

  void
  NodeDB::remove_stale_rcs()
  {
    auto cutoff_time =
        std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    cutoff_time -= router.is_service_node() ? RouterContact::OUTDATED_AGE : RouterContact::LIFETIME;
    for (auto itr = known_rcs.begin(); itr != known_rcs.end();)
    {
      if (cutoff_time > itr->second.timestamp())
      {
        log::info(logcat, "Pruning RC for {}, as it is too old to keep.", itr->first);
        known_rcs.erase(itr);
        continue;
      }
      itr++;
    }
  }

  bool
  NodeDB::put_rc(RemoteRC rc, rc_time now)
  {
    const auto& rid = rc.router_id();
    if (not want_rc(rid))
      return false;
    known_rcs.erase(rid);
    known_rcs.emplace(rid, std::move(rc));
    last_rc_update_times[rid] = now;
    return true;
  }

  size_t
  NodeDB::num_loaded() const
  {
    return router.loop()->call_get([this]() { return known_rcs.size(); });
  }

  bool
  NodeDB::put_rc_if_newer(RemoteRC rc, rc_time now)
  {
    auto itr = known_rcs.find(rc.router_id());
    if (itr == known_rcs.end() or itr->second.other_is_newer(rc))
    {
      return put_rc(std::move(rc), now);
    }
    return false;
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

      all.reserve(known_rcs.size());
      for (auto& entry : known_rcs)
      {
        all.push_back(&entry.second);
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

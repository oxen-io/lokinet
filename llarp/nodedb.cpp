#include "nodedb.hpp"

#include "crypto/types.hpp"
#include "dht/kademlia.hpp"
#include "messages/rc.hpp"
#include "messages/router_id.hpp"
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
      : _router{*r}
      , _root{std::move(root)}
      , _disk(std::move(diskCaller))
      , _next_flush_time{time_now_ms() + FlushInterval}
  {
    EnsureSkiplist(_root);
  }

  void
  NodeDB::Tick(llarp_time_t now)
  {
    if (_next_flush_time == 0s)
      return;

    if (now > _next_flush_time)
    {
      _router.loop()->call([this]() {
        _next_flush_time += FlushInterval;
        // make copy of all rcs
        std::vector<RemoteRC> copy;

        for (const auto& item : known_rcs)
          copy.push_back(item.second);

        // flush them to disk in one big job
        // TODO: split this up? idk maybe some day...
        _disk([this, data = std::move(copy)]() {
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
    return _root / skiplistDir / fname;
  }

  bool
  NodeDB::want_rc(const RouterID& rid) const
  {
    if (not _router.is_service_node())
      return true;
    return registered_routers.count(rid);
  }

  void
  NodeDB::check_bootstrap_state()
  {
    size_t active_count{0};
    size_t stale_count{0};

    auto now = time_now_ms();
    for (const auto& [rid, rc] : known_rcs)
    {
      if (not rc.is_outdated(now))
        active_count++;
      else 
        stale_count++;

      if (active_count > ROUTER_ID_SOURCE_COUNT)
        break;
    }

    if (active_count > ROUTER_ID_SOURCE_COUNT)
    {
      log::info(logcat, "We appear to be bootstrapped.");
      _bootstrapped = true;
      return;
    }
    _bootstrapped = false;
    if (stale_count > ROUTER_ID_SOURCE_COUNT)
    {
      log::info(logcat, "We need to soft-bootstrap from stale RCs.");
      // TODO: initiate soft-bootstrap
      return;
    }

    log::info(logcat, "We need to bootstrap from scratch.");
    bootstrap();
  }

  void
  NodeDB::bootstrap(size_t index)
  {
    // if we're here, our RC state is unusable; clear it
    known_rcs.clear();
    last_rc_update_times.clear();

    // bootstrapping has failed completely, inform Router to exit.
    if (index >= bootstrap_order.size())
    {
      log::error(logcat, "Bootstrapping has failed from all bootstraps; exiting.");
      _router.Stop();
      return;
    }
    const auto& rid = bootstrap_order[index];

    _router.link_manager().send_control_message(
        rid,
        "fetch_rcs",
        RCFetchMessage::serialize(rc_time::min()),
        [this, src = rid, index](oxen::quic::message m) {
        // TODO (Tom): DRY this out with the other invocations of fetch_rcs in here
          try
          {
            oxenc::bt_dict_consumer btdc{m.body()};
            if (not m)
            {
              auto reason = btdc.require<std::string_view>(messages::STATUS_KEY);
              log::info(logcat, "RC fetch to {} returned error: {}", src, reason);
            }
            else
            {
              auto btlc = btdc.require<oxenc::bt_list_consumer>("rcs"sv);
              auto timestamp = rc_time{std::chrono::seconds{btdc.require<int64_t>("time"sv)}};

              std::vector<RemoteRC> rcs;

              while (not btlc.is_finished())
              {
                rcs.emplace_back(btlc.consume_dict_consumer());
              }

              // TODO (Tom): add flag to mark these as coming from a bootstrap, rather
              // than an arbitrary relay.  A relay will still check that the RCs
              // match its registered relays lists; a client will just trust them.
              if (process_fetched_rcs(src, std::move(rcs), timestamp))
                return;
            }
          }
          catch (const std::exception& e)
          {
            log::info(logcat, "Failed to parse RC fetch response from {}: {}", src, e.what());
          }
          // failure, try next bootstrap
          log::warning(logcat, "Failed to bootstrap from {}, trying next.", src);
          bootstrap(index + 1);
        });
  }

  void
  NodeDB::set_bootstrap_routers(const std::set<RemoteRC>& rcs)
  {
    bootstraps.clear();  // this function really shouldn't be called more than once, but...
    for (const auto& rc : rcs)
    {
      bootstraps.emplace(rc.router_id(), rc);
      bootstrap_order.push_back(rc.router_id());
    }

    // so we use bootstraps in a random order
    std::shuffle(bootstrap_order.begin(), bootstrap_order.end(), llarp::csrng);
  }

  bool
  NodeDB::rotate_startup_rc_source()
  {
    if (active_client_routers.size() < 13)
    {
      // do something here
      return false;
    }

    RouterID temp = fetch_source;

    while (temp == fetch_source)
      std::sample(active_client_routers.begin(), active_client_routers.end(), &temp, 1, csrng);

    fetch_source = std::move(temp);
    return true;
  }

  /// Called in normal operation when the relay we fetched RCs from gives either a "bad"
  /// response or a timeout.  Attempts to switch to a new relay as our RC source, using
  /// existing connections if possible, and respecting pinned edges.
  void
  NodeDB::rotate_rc_source()
  {
    auto conn_count = _router.link_manager().get_num_connected();

    // This function makes no sense to be called if we have no connections...
    if (conn_count == 0)
      throw std::runtime_error{"Called rotate_rc_source with no connections, does not make sense!"};

    // We should not be in this function if client_known_routers isn't populated
    if (active_client_routers.size() <= 1)
      throw std::runtime_error{"Cannot rotate RC source without RC source(s) to rotate to!"};

    RemoteRC new_source{};
    _router.link_manager().get_random_connected(new_source);

    if (conn_count == 1)
    {
      // if we only have one connection, it must be current rc fetch source
      assert(new_source.router_id() == fetch_source);

      if (pinned_edges.size() == 1)
      {
        // only one pinned edge set, use it even though it gave unsatisfactory RCs
        assert(fetch_source == *(pinned_edges.begin()));
        log::warning(
            logcat,
            "Single pinned edge {} gave bad RC response; still using it despite this.",
            fetch_source);
        return;
      }

      // only one connection, choose a new relay to connect to for rc fetching
      RouterID r = fetch_source;

      while (r == fetch_source)
      {
        std::sample(active_client_routers.begin(), active_client_routers.end(), &r, 1, csrng);
      }
      fetch_source = std::move(r);
      return;
    }

    // choose one of our other existing connections to use as the RC fetch source
    while (new_source.router_id() == fetch_source)
    {
      _router.link_manager().get_random_connected(new_source);
    }

    fetch_source = new_source.router_id();
  }

  // TODO: trust model
  bool
  NodeDB::process_fetched_rcs(RouterID source, std::vector<RemoteRC> rcs, rc_time timestamp)
  {
    fetch_source = source;

    /*
        TODO: trust model analyzing returned list of RCs
    */

    for (auto& rc : rcs)
      put_rc_if_newer(std::move(rc), timestamp);

    last_rc_update_relay_timestamp = timestamp;

    return true;
  }

  void
  NodeDB::ingest_rid_fetch_responses(const RouterID& source, std::vector<RouterID> ids)
  {
    fetch_rid_responses[source] = std::move(ids);
  }

  // TODO: trust model
  bool
  NodeDB::process_fetched_rids()
  {
    for (const auto& [rid, responses] : fetch_rid_responses)
    {
      // TODO: empty == failure, handle that case
      for (const auto& response : responses)
      {
        active_client_routers.insert(std::move(response));
      }
    }

    return true;
  }

  void
  NodeDB::fetch_initial()
  {
    int num_fails = 0;

    // fetch_initial_{rcs,router_ids} return false when num_fails == 0
    if (fetch_initial_rcs(num_fails))
    {
      _router.last_rc_fetch = llarp::time_point_now();

      if (fetch_initial_router_ids(num_fails))
      {
        _router.last_rid_fetch = llarp::time_point_now();
        return;
      }
    }

    // failure case
    // TODO: use bootstrap here!
  }

  bool
  NodeDB::fetch_initial_rcs(int n_fails)
  {
    int num_failures = n_fails;
    is_fetching_rcs = true;
    std::vector<RouterID> needed;
    const auto now = time_point_now();

    for (const auto& [rid, rc] : known_rcs)
    {
      if (now - rc.timestamp() > RouterContact::OUTDATED_AGE)
        needed.push_back(rid);
    }

    RouterID src =
        *std::next(active_client_routers.begin(), csrng() % active_client_routers.size());

    while (num_failures < MAX_FETCH_ATTEMPTS)
    {
      auto success = std::make_shared<std::promise<bool>>();
      auto f = success->get_future();

      _router.link_manager().fetch_rcs(
          src,
          RCFetchMessage::serialize(last_rc_update_relay_timestamp, needed),
          [this, src, p = std::move(success)](oxen::quic::message m) mutable {
            if (m.timed_out)
            {
              log::info(logcat, "RC fetch to {} timed out", src);
              p->set_value(false);
              return;
            }
            try
            {
              oxenc::bt_dict_consumer btdc{m.body()};
              if (not m)
              {
                auto reason = btdc.require<std::string_view>(messages::STATUS_KEY);
                log::info(logcat, "RC fetch to {} returned error: {}", src, reason);
                p->set_value(false);
                return;
              }

              auto btlc = btdc.require<oxenc::bt_list_consumer>("rcs"sv);
              auto timestamp = rc_time{std::chrono::seconds{btdc.require<int64_t>("time"sv)}};

              std::vector<RemoteRC> rcs;

              while (not btlc.is_finished())
              {
                rcs.emplace_back(btlc.consume_dict_consumer());
              }

              process_fetched_rcs(src, std::move(rcs), timestamp);
              p->set_value(true);
            }
            catch (const std::exception& e)
            {
              log::info(logcat, "Failed to parse RC fetch response from {}: {}", src, e.what());
              p->set_value(false);
              return;
            }
          });

      if (f.get())
      {
        log::debug(logcat, "Successfully fetched RC's from {}", src);
        fetch_source = src;
        return true;
      }

      ++num_failures;
      log::debug(
          logcat,
          "Unable to fetch RC's from {}; rotating RC source ({}/{} attempts)",
          src,
          num_failures,
          MAX_FETCH_ATTEMPTS);

      src = *std::next(active_client_routers.begin(), csrng() % active_client_routers.size());
    }

    return false;
  }

  bool
  NodeDB::fetch_initial_router_ids(int n_fails)
  {
    assert(not is_fetching_rids);

    int num_failures = n_fails;
    select_router_id_sources();

    is_fetching_rids = true;
    fetch_rid_responses.clear();

    RouterID src =
        *std::next(active_client_routers.begin(), csrng() % active_client_routers.size());

    std::unordered_set<RouterID> fails;

    while (num_failures < MAX_FETCH_ATTEMPTS)
    {
      auto success = std::make_shared<std::promise<int>>();
      auto f = success->get_future();
      fails.clear();

      for (const auto& target : rid_sources)
      {
        _router.link_manager().fetch_router_ids(
            src,
            RouterIDFetch::serialize(target),
            [this, src, target, p = std::move(success)](oxen::quic::message m) mutable {
              if (not m)
              {
                log::info(link_cat, "RID fetch from {} via {} timed out", src, target);

                ingest_rid_fetch_responses(src);
                p->set_value(-1);
                return;
              }

              try
              {
                oxenc::bt_dict_consumer btdc{m.body()};

                btdc.required("routers");
                auto router_id_strings = btdc.consume_list<std::vector<ustring>>();

                btdc.require_signature("signature", [&src](ustring_view msg, ustring_view sig) {
                  if (sig.size() != 64)
                    throw std::runtime_error{"Invalid signature: not 64 bytes"};
                  if (not crypto::verify(src, msg, sig))
                    throw std::runtime_error{
                        "Failed to verify signature for fetch RouterIDs response."};
                });

                std::vector<RouterID> router_ids;

                for (const auto& s : router_id_strings)
                {
                  if (s.size() != RouterID::SIZE)
                  {
                    log::warning(
                        link_cat, "RID fetch from {} via {} returned bad RouterID", target, src);
                    p->set_value(0);
                    return;
                  }

                  router_ids.emplace_back(s.data());
                }

                ingest_rid_fetch_responses(src, std::move(router_ids));
                return;
              }
              catch (const std::exception& e)
              {
                log::info(link_cat, "Error handling fetch RouterIDs response: {}", e.what());
                p->set_value(0);
              }

              ingest_rid_fetch_responses(src);  // empty response == failure
            });

        switch (f.get())
        {
          case 1:
            log::debug(logcat, "Successfully fetched RID's from {} via {}", target, src);
            continue;
          case 0:
            // RC node relayed our fetch routerID request, but the request failed at the target
            log::debug(logcat, "Unsuccessfully fetched RID's from {} via {}", target, src);
            fails.insert(target);
            continue;
          default:
            // RC node failed to relay our routerID request; re-select RC node and continue
            log::debug(logcat, "RC source {} failed to mediate RID fetching from {}", src, target);
            src = *std::next(active_client_routers.begin(), csrng() % active_client_routers.size());
            ++num_failures;
            fetch_rcs();
            continue;
        }
      }

      auto n_fails = fails.size();

      if (n_fails <= MAX_RID_ERRORS)
      {
        log::debug(
            logcat,
            "RID fetching was successful ({}/{} acceptable errors)",
            fails.size(),
            MAX_RID_ERRORS);
        fetch_source = src;

        // this is where the trust model will do verification based on the similarity of the sets
        if (process_fetched_rids())
        {
          log::debug(logcat, "Accumulated RID's accepted by trust model");
          return true;
        }

        log::debug(
            logcat, "Accumulated RID's rejected by trust model, reselecting all RID sources...");
        select_router_id_sources(rid_sources);
        ++num_failures;
        continue;
      }

      // we had 4 or more failed requests, so we will need to rotate our rid sources
      log::debug(
          logcat, "RID fetching found {} failures; reselecting failed RID sources...", n_fails);
      ++num_failures;
      select_router_id_sources(fails);
    }

    return false;
  }

  void
  NodeDB::fetch_rcs()
  {
    auto& num_failures = fetch_failures;

    // base case; this function is called recursively
    if (num_failures > MAX_FETCH_ATTEMPTS)
    {
      fetch_rcs_result(true);
      return;
    }

    is_fetching_rcs = true;

    std::vector<RouterID> needed;
    const auto now = time_point_now();

    for (const auto& [rid, rc] : known_rcs)
    {
      if (now - rc.timestamp() > RouterContact::OUTDATED_AGE)
        needed.push_back(rid);
    }

    RouterID& src = fetch_source;

    _router.link_manager().fetch_rcs(
        src,
        RCFetchMessage::serialize(last_rc_update_relay_timestamp, needed),
        [this, src](oxen::quic::message m) mutable {
          if (m.timed_out)
          {
            log::info(logcat, "RC fetch to {} timed out", src);
            fetch_rcs_result(true);
            return;
          }
          try
          {
            oxenc::bt_dict_consumer btdc{m.body()};
            if (not m)
            {
              auto reason = btdc.require<std::string_view>(messages::STATUS_KEY);
              log::info(logcat, "RC fetch to {} returned error: {}", src, reason);
              fetch_rcs_result(true);
              return;
            }

            auto btlc = btdc.require<oxenc::bt_list_consumer>("rcs"sv);
            auto timestamp = rc_time{std::chrono::seconds{btdc.require<int64_t>("time"sv)}};

            std::vector<RemoteRC> rcs;

            while (not btlc.is_finished())
            {
              rcs.emplace_back(btlc.consume_dict_consumer());
            }

            // if process_fetched_rcs returns false, then the trust model rejected the fetched RC's
            fetch_rcs_result(not process_fetched_rcs(src, std::move(rcs), timestamp));
          }
          catch (const std::exception& e)
          {
            log::info(logcat, "Failed to parse RC fetch response from {}: {}", src, e.what());
            fetch_rcs_result(true);
            return;
          }
        });
  }

  void
  NodeDB::fetch_rcs_result(bool error)
  {
    if (error)
    {
      ++fetch_failures;

      if (fetch_failures > MAX_FETCH_ATTEMPTS)
      {
        log::info(
            logcat,
            "Failed {} attempts to fetch RC's from {}; reverting to bootstrap...",
            MAX_FETCH_ATTEMPTS,
            fetch_source);
        // TODO: revert to bootstrap
        // set rc_fetch_source to bootstrap and try again!
      }
      else
        // find new non-bootstrap RC fetch source and try again buddy
        fetch_source = std::next(known_rcs.begin(), csrng() % known_rcs.size())->first;

      fetch_rcs();
    }
    else
    {
      log::debug(logcat, "Successfully fetched RC's from {}", fetch_source);
      post_fetch_rcs();
    }
  }

  void
  NodeDB::post_fetch_rcs()
  {
    is_fetching_rcs = false;
    _router.last_rc_fetch = llarp::time_point_now();
  }

  // TODO: differentiate between errors from the relay node vs errors from the target nodes
  void
  NodeDB::fetch_router_ids()
  {
    // base case; this function is called recursively
    if (fetch_failures > MAX_FETCH_ATTEMPTS)
    {
      fetch_rids_result();
      return;
    }

    if (rid_sources.empty())
      select_router_id_sources();

    // if we *still* don't have fetch sources, we can't exactly fetch...
    if (rid_sources.empty())
    {
      log::error(logcat, "Attempting to fetch RouterIDs, but have no source from which to do so.");
      return;
    }

    is_fetching_rids = true;
    fetch_rid_responses.clear();

    RouterID& src = fetch_source;

    for (const auto& target : rid_sources)
    {
      _router.link_manager().fetch_router_ids(
          src,
          RouterIDFetch::serialize(target),
          [this, src, target](oxen::quic::message m) mutable {
            if (m.timed_out)
            {
              log::info(link_cat, "RID fetch from {} via {} timed out", src, target);

              ++fetch_failures;
              ingest_rid_fetch_responses(src);
              fetch_rids_result();
              return;
            }

            try
            {
              oxenc::bt_dict_consumer btdc{m.body()};

              btdc.required("routers");
              auto router_id_strings = btdc.consume_list<std::vector<ustring>>();

              btdc.require_signature("signature", [&src](ustring_view msg, ustring_view sig) {
                if (sig.size() != 64)
                  throw std::runtime_error{"Invalid signature: not 64 bytes"};
                if (not crypto::verify(src, msg, sig))
                  throw std::runtime_error{
                      "Failed to verify signature for fetch RouterIDs response."};
              });

              std::vector<RouterID> router_ids;

              for (const auto& s : router_id_strings)
              {
                if (s.size() != RouterID::SIZE)
                {
                  log::warning(
                      link_cat, "RID fetch from {} via {} returned bad RouterID", target, src);
                  ingest_rid_fetch_responses(target);
                  fail_sources.insert(target);
                  fetch_rids_result();
                  return;
                }

                router_ids.emplace_back(s.data());
              }

              ingest_rid_fetch_responses(target, std::move(router_ids));
              fetch_rids_result();  // success
              return;
            }
            catch (const std::exception& e)
            {
              log::info(link_cat, "Error handling fetch RouterIDs response: {}", e.what());
              ingest_rid_fetch_responses(target);
              fail_sources.insert(target);
              fetch_rids_result();
            }
          });
    }
  }

  void
  NodeDB::fetch_rids_result()
  {
    if (fetch_failures > MAX_FETCH_ATTEMPTS)
    {
      log::info(
          logcat,
          "Failed {} attempts to fetch RID's from {}; reverting to bootstrap...",
          MAX_FETCH_ATTEMPTS,
          fetch_source);

      // TODO: revert rc_source to bootstrap, start over

      fetch_router_ids();
      return;
    }

    auto n_responses = fetch_rid_responses.size();

    if (n_responses < MIN_RID_FETCHES)
    {
      log::debug(logcat, "Received {}/{} fetch RID requests", n_responses, 12);
      return;
    }

    auto n_fails = fail_sources.size();

    if (n_fails <= MAX_RID_ERRORS)
    {
      log::debug(
          logcat, "RID fetching was successful ({}/{} acceptable errors)", n_fails, MAX_RID_ERRORS);

      // this is where the trust model will do verification based on the similarity of the sets
      if (process_fetched_rids())
      {
        log::debug(logcat, "Accumulated RID's accepted by trust model");
        post_fetch_rids();
        return;
      }

      log::debug(
          logcat, "Accumulated RID's rejected by trust model, reselecting all RID sources...");
      select_router_id_sources(rid_sources);
      ++fetch_failures;
    }
    else
    {
      // we had 4 or more failed requests, so we will need to rotate our rid sources
      log::debug(
          logcat, "RID fetching found {} failures; reselecting failed RID sources...", n_fails);
      ++fetch_failures;
      select_router_id_sources(fail_sources);
    }

    fetch_router_ids();
  }

  void
  NodeDB::post_fetch_rids()
  {
    is_fetching_rids = false;
    fetch_rid_responses.clear();
    fail_sources.clear();
    fetch_failures = 0;
    _router.last_rid_fetch = llarp::time_point_now();
  }

  void
  NodeDB::select_router_id_sources(std::unordered_set<RouterID> excluded)
  {
    // bootstrapping should be finished before this is called, so this
    // shouldn't happen; need to make sure that's the case.
    if (active_client_routers.empty())
      return;

    // in case we pass the entire list
    std::unordered_set<RouterID> temp = rid_sources;

    // keep using any we've been using, but remove `excluded` ones
    if (excluded == rid_sources)
      temp.clear();
    else
    {
      for (const auto& r : excluded)
        temp.erase(r);
    }

    // only know so many routers, so no need to randomize
    if (active_client_routers.size() <= (ROUTER_ID_SOURCE_COUNT + excluded.size()))
    {
      for (const auto& r : active_client_routers)
      {
        if (excluded.count(r))
          continue;
        temp.insert(r);
      }
    }

    // select at random until we have chosen enough
    while (temp.size() < ROUTER_ID_SOURCE_COUNT)
    {
      RouterID r;
      std::sample(active_client_routers.begin(), active_client_routers.end(), &r, 1, csrng);
      if (excluded.count(r) == 0)
        temp.insert(r);
    }

    rid_sources.swap(temp);
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

    for (auto itr = known_rcs.begin(); itr != known_rcs.end();)
    {
      if (registered_routers.count(itr->first) == 0)
      {
        log::debug(logcat, "Removing no-longer-registered RC with RouterID {}", itr->first);
        known_rcs.erase(itr);
      }
      itr++;
    }

    check_bootstrap_state();
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

    if (not _router.is_service_node())
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
    if (_root.empty())
      return;

    std::set<fs::path> purge;

    const auto now = time_now_ms();

    for (const char& ch : skiplist_subdirs)
    {
      if (!ch)
        continue;
      std::string p;
      p += ch;
      fs::path sub = _root / p;

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

        const auto& rid = rc.router_id();

        known_rcs.emplace(rid, rc);
        // TODO: the list of relays should be maintained and stored separately from
        // the RCs, as we keep older RCs around in case we go offline and need to
        // bootstrap, but they shouldn't be in the "good relays" list.
        active_client_routers.insert(rid);

        return true;
      });
    }

    if (not purge.empty())
    {
      log::warning(logcat, "removing {} invalid RCs from disk", purge.size());

      for (const auto& fpath : purge)
        fs::remove(fpath);
    }

    // (client-only) after loading RCs, check if we're in a usable state
    // relay will do this after set_router_whitelist, as it gets its RouterID list
    // from oxend and needs that before it can decide.
    if (not _router.is_service_node())
      check_bootstrap_state();
  }

  void
  NodeDB::save_to_disk() const
  {
    if (_root.empty())
      return;

    _router.loop()->call([this]() {
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
    _router.loop()->call([this, pk]() {
      known_rcs.erase(pk);
      remove_many_from_disk_async({pk});
    });
  }

  void
  NodeDB::remove_stale_rcs()
  {
    auto cutoff_time = time_point_now();

    cutoff_time -=
        _router.is_service_node() ? RouterContact::OUTDATED_AGE : RouterContact::LIFETIME;
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
    return _router.loop()->call_get([this]() { return known_rcs.size(); });
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
    if (_root.empty())
      return;
    // build file list
    std::set<fs::path> files;
    for (auto id : remove)
    {
      files.emplace(get_path_by_pubkey(std::move(id)));
    }
    // remove them from the disk via the diskio thread
    _disk([files]() {
      for (auto fpath : files)
        fs::remove(fpath);
    });
  }

  RemoteRC
  NodeDB::find_closest_to(llarp::dht::Key_t location) const
  {
    return _router.loop()->call_get([this, location]() -> RemoteRC {
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
    return _router.loop()->call_get([this, location, numRouters]() -> std::vector<RemoteRC> {
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

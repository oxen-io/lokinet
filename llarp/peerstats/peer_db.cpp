#include "peer_db.hpp"

#include <llarp/util/logging/logger.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/str.hpp>

namespace llarp
{
  PeerDb::PeerDb()
  {
    m_lastFlush.store({});
  }

  void
  PeerDb::loadDatabase(std::optional<fs::path> file)
  {
    std::lock_guard guard(m_statsLock);

    if (m_storage)
      throw std::runtime_error("Reloading database not supported");  // TODO

    m_peerStats.clear();

    // sqlite_orm treats empty-string as an indicator to load a memory-backed database, which we'll
    // use if file is an empty-optional
    std::string fileString;
    if (file.has_value())
    {
      fileString = file->string();
      LogInfo("Loading PeerDb from file ", fileString);
    }
    else
    {
      LogInfo("Loading memory-backed PeerDb");
    }

    m_storage = std::make_unique<PeerDbStorage>(initStorage(fileString));
    m_storage->sync_schema(true);  // true for "preserve" as in "don't nuke" (how cute!)

    auto allStats = m_storage->get_all<PeerStats>();
    LogInfo("Loading ", allStats.size(), " PeerStats from table peerstats...");
    for (PeerStats& stats : allStats)
    {
      // we cleared m_peerStats, and the database should enforce that routerId is unique...
      assert(m_peerStats.find(stats.routerId) == m_peerStats.end());

      stats.stale = false;
      m_peerStats[stats.routerId] = stats;
    }
  }

  void
  PeerDb::flushDatabase()
  {
    LogDebug("flushing PeerDb...");

    auto start = time_now_ms();
    if (not shouldFlush(start))
    {
      LogWarn("Call to flushDatabase() while already in progress, ignoring");
      return;
    }

    if (not m_storage)
      throw std::runtime_error("Cannot flush database before it has been loaded");

    std::vector<PeerStats> staleStats;

    {
      std::lock_guard guard(m_statsLock);

      // copy all stale entries
      for (auto& entry : m_peerStats)
      {
        if (entry.second.stale)
        {
          staleStats.push_back(entry.second);
          entry.second.stale = false;
        }
      }
    }

    LogDebug("Updating ", staleStats.size(), " stats");

    {
      auto guard = m_storage->transaction_guard();

      for (const auto& stats : staleStats)
      {
        m_storage->replace(stats);
      }

      guard.commit();
    }

    auto end = time_now_ms();

    auto elapsed = end - start;
    LogDebug("PeerDb flush took about ", elapsed, " seconds");

    m_lastFlush.store(end);
  }

  void
  PeerDb::accumulatePeerStats(const RouterID& routerId, const PeerStats& delta)
  {
    if (routerId != delta.routerId)
      throw std::invalid_argument(
          stringify("routerId ", routerId, " doesn't match ", delta.routerId));

    std::lock_guard guard(m_statsLock);
    auto itr = m_peerStats.find(routerId);
    if (itr == m_peerStats.end())
      itr = m_peerStats.insert({routerId, delta}).first;
    else
      itr->second += delta;

    itr->second.stale = true;
  }

  void
  PeerDb::modifyPeerStats(const RouterID& routerId, std::function<void(PeerStats&)> callback)
  {
    std::lock_guard guard(m_statsLock);

    PeerStats& stats = m_peerStats[routerId];
    stats.routerId = routerId;
    stats.stale = true;
    callback(stats);
  }

  std::optional<PeerStats>
  PeerDb::getCurrentPeerStats(const RouterID& routerId) const
  {
    std::lock_guard guard(m_statsLock);
    auto itr = m_peerStats.find(routerId);
    if (itr == m_peerStats.end())
      return std::nullopt;
    else
      return itr->second;
  }

  std::vector<PeerStats>
  PeerDb::listAllPeerStats() const
  {
    std::lock_guard guard(m_statsLock);

    std::vector<PeerStats> statsList;
    statsList.reserve(m_peerStats.size());

    for (const auto& [routerId, stats] : m_peerStats)
    {
      statsList.push_back(stats);
    }

    return statsList;
  }

  std::vector<PeerStats>
  PeerDb::listPeerStats(const std::vector<RouterID>& ids) const
  {
    std::lock_guard guard(m_statsLock);

    std::vector<PeerStats> statsList;
    statsList.reserve(ids.size());

    for (const auto& id : ids)
    {
      const auto itr = m_peerStats.find(id);
      if (itr != m_peerStats.end())
        statsList.push_back(itr->second);
    }

    return statsList;
  }

  /// Assume we receive an RC at some point `R` in time which was signed at some point `S` in time
  /// and expires at some point `E` in time, as depicted below:
  ///
  /// +-----------------------------+
  /// | signed rc                   |  <- useful lifetime of RC
  /// +-----------------------------+
  /// ^  [ . . . . . . . . ] <----------- window in which we receive this RC gossiped to us
  /// |  ^                          ^
  /// |  |                          |
  /// S  R                          E
  ///
  /// One useful metric from this is the difference between (E - R), the useful contact time of this
  /// RC. As we track this metric over time, the high and low watermarks serve to tell us how
  /// quickly we receive signed RCs from a given router and how close to expiration they are when
  /// we receive them. The latter is particularly useful, and should always be a positive number for
  /// a healthy router. A negative number indicates that we are receiving an expired RC.
  ///
  /// TODO: we actually discard expired RCs, so we currently would not detect a negative value for
  ///       (E - R)
  ///
  /// Another related metric is the distance between a newly received RC and the previous RC's
  /// expiration, which represents how close we came to having no useful RC to work with. This
  /// should be a high (positive) number for a healthy router, and if negative indicates that we
  /// had no way to contact this router for a period of time.
  ///
  ///                               E1      E2      E3
  ///                               |       |       |
  ///                               v       |       |
  /// +-----------------------------+       |       |
  /// | signed rc 1                 |       |       |
  /// +-----------------------------+       |       |
  ///    [ . . . . . ]                      v       |
  ///    ^    +-----------------------------+       |
  ///    |    | signed rc 2                 |       |
  ///    |    +-----------------------------+       |
  ///    |      [ . . . . . . . . . . ]             v
  ///    |      ^     +-----------------------------+
  ///    |      |     | signed rc 3                 |
  ///    |      |     +-----------------------------+
  ///    |      |                              [ . . ]
  ///    |      |                              ^
  ///    |      |                              |
  ///    R1     R2                             R3
  ///
  /// Example: the delta between (E1 - R2) is healthy, but the delta between (E2 - R3) is indicates
  /// that we had a brief period of time where we had no valid (non-expired) RC for this router
  /// (because it is negative).
  void
  PeerDb::handleGossipedRC(const RouterContact& rc, llarp_time_t now)
  {
    std::lock_guard guard(m_statsLock);

    RouterID id(rc.pubkey);
    auto& stats = m_peerStats[id];
    stats.routerId = id;

    const bool isNewRC = (stats.lastRCUpdated < rc.last_updated);

    if (isNewRC)
    {
      stats.numDistinctRCsReceived++;

      if (stats.numDistinctRCsReceived > 1)
      {
        auto prevRCExpiration = (stats.lastRCUpdated + RouterContact::Lifetime);

        // we track max expiry as the delta between (last expiration time - time received),
        // and this value will be negative for an unhealthy router
        // TODO: handle case where new RC is also expired? just ignore?
        auto expiry = prevRCExpiration - now;

        if (stats.numDistinctRCsReceived == 2)
          stats.leastRCRemainingLifetime = expiry;
        else
          stats.leastRCRemainingLifetime = std::min(stats.leastRCRemainingLifetime, expiry);
      }

      stats.lastRCUpdated = rc.last_updated;
      stats.stale = true;
    }
  }

  void
  PeerDb::configure(const RouterConfig& routerConfig)
  {
    fs::path dbPath = routerConfig.m_dataDir / "peerstats.sqlite";

    loadDatabase(dbPath);
  }

  bool
  PeerDb::shouldFlush(llarp_time_t now)
  {
    constexpr llarp_time_t TargetFlushInterval = 30s;
    return (now - m_lastFlush.load() >= TargetFlushInterval);
  }

  util::StatusObject
  PeerDb::ExtractStatus() const
  {
    std::lock_guard guard(m_statsLock);

    bool loaded = (m_storage.get() != nullptr);
    util::StatusObject dbFile = nullptr;
    if (loaded)
      dbFile = m_storage->filename();

    std::vector<util::StatusObject> statsObjs;
    statsObjs.reserve(m_peerStats.size());
    for (const auto& pair : m_peerStats)
    {
      statsObjs.push_back(pair.second.toJson());
    }

    util::StatusObject obj{
        {"dbLoaded", loaded},
        {"dbFile", dbFile},
        {"lastFlushMs", m_lastFlush.load().count()},
        {"stats", statsObjs},
    };
    return obj;
  }

};  // namespace llarp

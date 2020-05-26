#include <peerstats/peer_db.hpp>

#include <util/logging/logger.hpp>
#include <util/str.hpp>

namespace llarp
{
  PeerDb::PeerDb()
  {
    m_lastFlush.store({});
  }

  void
  PeerDb::loadDatabase(std::optional<std::filesystem::path> file)
  {
    std::lock_guard gaurd(m_statsLock);

    m_peerStats.clear();

    if (m_storage)
      throw std::runtime_error("Reloading database not supported");  // TODO

    // sqlite_orm treats empty-string as an indicator to load a memory-backed database, which we'll
    // use if file is an empty-optional
    std::string fileString;
    if (file.has_value())
    {
      fileString = file.value().native();
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
    for (const PeerStats& stats : allStats)
    {
      RouterID id;
      if (not id.FromString(stats.routerId))
        throw std::runtime_error(
            stringify("Database contains invalid PeerStats with id ", stats.routerId));

      m_peerStats[id] = stats;
    }
  }

  void
  PeerDb::flushDatabase()
  {
    LogDebug("flushing PeerDb...");

    auto start = time_now_ms();
    if (not shouldFlush(start))
      LogWarn("Double PeerDb flush?");

    if (not m_storage)
      throw std::runtime_error("Cannot flush database before it has been loaded");

    decltype(m_peerStats) copy;

    {
      std::lock_guard gaurd(m_statsLock);
      copy = m_peerStats;  // expensive deep copy
    }

    for (const auto& entry : copy)
    {
      // call me paranoid...
      assert(not entry.second.routerId.empty());
      assert(entry.first.ToString() == entry.second.routerId);

      m_storage->replace(entry.second);
    }

    auto end = time_now_ms();

    auto elapsed = end - start;
    LogDebug("PeerDb flush took about ", elapsed, " millis");

    m_lastFlush.store(end);
  }

  void
  PeerDb::accumulatePeerStats(const RouterID& routerId, const PeerStats& delta)
  {
    if (routerId.ToString() != delta.routerId)
      throw std::invalid_argument(
          stringify("routerId ", routerId, " doesn't match ", delta.routerId));

    std::lock_guard gaurd(m_statsLock);
    auto itr = m_peerStats.find(routerId);
    if (itr == m_peerStats.end())
      itr = m_peerStats.insert({routerId, delta}).first;
    else
      itr->second += delta;
  }

  void
  PeerDb::modifyPeerStats(const RouterID& routerId, std::function<void(PeerStats&)> callback)
  {
    std::lock_guard gaurd(m_statsLock);

    PeerStats& stats = m_peerStats[routerId];
    stats.routerId = routerId.ToString();
    callback(stats);
  }

  std::optional<PeerStats>
  PeerDb::getCurrentPeerStats(const RouterID& routerId) const
  {
    std::lock_guard gaurd(m_statsLock);
    auto itr = m_peerStats.find(routerId);
    if (itr == m_peerStats.end())
      return std::nullopt;
    else
      return itr->second;
  }

  void
  PeerDb::configure(const RouterConfig& routerConfig)
  {
    if (not routerConfig.m_enablePeerStats)
      throw std::runtime_error("[router]:enable-peer-stats is not enabled");

    fs::path dbPath = routerConfig.m_dataDir / "peerstats.sqlite";

    loadDatabase(dbPath);
  }

  bool
  PeerDb::shouldFlush(llarp_time_t now)
  {
    constexpr llarp_time_t TargetFlushInterval = 30s;
    return (now - m_lastFlush.load() >= TargetFlushInterval);
  }

};  // namespace llarp

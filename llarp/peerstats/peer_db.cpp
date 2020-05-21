#include <peerstats/peer_db.hpp>

#include <util/logging/logger.hpp>
#include <util/str.hpp>

namespace llarp
{
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

};  // namespace llarp

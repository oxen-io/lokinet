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
      fileString = file.value().native();

    m_storage = std::make_unique<PeerDbStorage>(initStorage(fileString));
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

    for (const auto& entry : m_peerStats)
    {
      // call me paranoid...
      assert(not entry.second.routerIdHex.empty());
      assert(entry.first.ToHex() == entry.second.routerIdHex);

      m_storage->insert(entry.second);
    }
  }

  void
  PeerDb::accumulatePeerStats(const RouterID& routerId, const PeerStats& delta)
  {
    if (routerId.ToHex() != delta.routerIdHex)
      throw std::invalid_argument(
          stringify("routerId ", routerId, " doesn't match ", delta.routerIdHex));

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

#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>

#include <llarp/util/fs.hpp>
#include <llarp/config/config.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/status.hpp>
#include "types.hpp"
#ifdef LOKINET_PEERSTATS_BACKEND
#include "orm.hpp"
#endif

namespace llarp
{
  /// Maintains a database of stats collected about the connections with our Service Node peers.
  /// This uses a sqlite3 database behind the scenes as persistance, but this database is
  /// periodically flushed to, meaning that it will become stale as PeerDb accumulates stats without
  /// a flush.
  struct PeerDb
  {
    /// Constructor
    PeerDb();

    /// Loads the database from disk using the provided filepath. If the file is equal to
    /// `std::nullopt`, the database will be loaded into memory (useful for testing).
    ///
    /// This must be called prior to calling flushDatabase(), and will truncate any existing data.
    ///
    /// This is a blocking call, both in the sense that it blocks on disk/database I/O and that it
    /// will sit on a mutex while the database is loaded.
    ///
    /// @param file is an optional file which doesn't have to exist but must be writable, if a value
    ///        is provided. If no value is provided, the database will be memory-backed.
    /// @throws if sqlite_orm/sqlite3 is unable to open or create a database at the given file
    void
    loadDatabase(std::optional<fs::path> file);

    /// Flushes the database. Must be called after loadDatabase(). This call will block during I/O
    /// and should be called in an appropriate threading context. However, it will make a temporary
    /// copy of the peer stats so as to avoid sitting on a mutex lock during disk I/O.
    ///
    /// @throws if the database could not be written to (esp. if loadDatabase() has not been called)
    void
    flushDatabase();

    /// Add the given stats to the cummulative stats for the given peer. For cummulative stats, the
    /// stats are added together; for watermark stats, the max is kept.
    ///
    /// This is intended to be used in the following pattern:
    ///
    /// 1) Initialize an empty PeerStats
    /// 2) Collect relevant stats
    /// 3) Call accumulatePeerStats() with the stats
    /// 4) Reset the stats to 0
    /// 5) <Repeat 2-4 periodically>
    ///
    /// @param routerId is the id of the router whose stats should be modified.
    /// @param delta is the stats to add to the existing stats
    void
    accumulatePeerStats(const RouterID& routerId, const PeerStats& delta);

    /// Allows write-access to the stats for a given peer while appropriate mutex lock is held. This
    /// is an alternative means of incrementing peer stats that is suitable for one-off
    /// modifications.
    ///
    /// Note that this holds m_statsLock during the callback invocation, so the callback should
    /// return as quickly as possible.
    ///
    /// @param routerId is the id of the router whose stats should be modified.
    /// @param callback is a function which will be called immediately with mutex held
    void
    modifyPeerStats(const RouterID& routerId, std::function<void(PeerStats&)> callback);

    /// Provides a snapshot of the most recent PeerStats we have for the given peer. If we don't
    /// have any stats for the peer, std::nullopt
    ///
    /// @param routerId is the RouterID of the requested peer
    /// @return a copy of the most recent peer stats or an empty one if no such peer is known
    std::optional<PeerStats>
    getCurrentPeerStats(const RouterID& routerId) const;

    /// Lists all peer stats. This essentially dumps the database into a list of PeerStats objects.
    ///
    /// Note that this avoids disk I/O by copying from our cached map of peers.
    ///
    /// @return a list of all PeerStats we have maintained
    std::vector<PeerStats>
    listAllPeerStats() const;

    /// Lists specific peer stats.
    ///
    /// @param peers is list of RouterIDs which are desired
    /// @return a list of the requested peers. Peers not found will be omitted.
    std::vector<PeerStats>
    listPeerStats(const std::vector<RouterID>& ids) const;

    /// Handles a new gossiped RC, updating stats as needed. The database tracks the last
    /// advertised update time, so it knows whether this is a new RC or not.
    ///
    /// The given RC is assumed to be valid.
    ///
    /// @param rc is the RouterContact to handle
    /// @param now is an optional time representing the current time
    void
    handleGossipedRC(const RouterContact& rc, llarp_time_t now = time_now_ms());

    /// Configures the PeerDb based on RouterConfig
    ///
    /// @param routerConfig
    void
    configure(const RouterConfig& routerConfig);

    /// Returns whether or not we should flush, as determined by the last time we flushed and the
    /// configured flush interval.
    ///
    /// @param now is the current[-ish] time
    bool
    shouldFlush(llarp_time_t now);

    /// Get JSON status for API
    ///
    /// @return JSON object representing our current status
    util::StatusObject
    ExtractStatus() const;

#ifdef LOKINET_PEERSTATS_BACKEND
   private:
    std::unordered_map<RouterID, PeerStats> m_peerStats;
    mutable std::mutex m_statsLock;

    std::unique_ptr<PeerDbStorage> m_storage;

    std::atomic<llarp_time_t> m_lastFlush;
#endif
  };

}  // namespace llarp

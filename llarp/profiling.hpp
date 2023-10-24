#pragma once

#include "path/path.hpp"
#include "router_id.hpp"
#include "util/bencode.hpp"
#include "util/thread/annotations.hpp"
#include "util/thread/threading.hpp"

#include <map>

namespace oxenc
{
  class bt_dict_consumer;
  class bt_dict_producer;
}  // namespace oxenc

namespace llarp
{
  struct RouterProfile
  {
    static constexpr size_t MaxSize = 256;
    uint64_t connectTimeoutCount = 0;
    uint64_t connectGoodCount = 0;
    uint64_t pathSuccessCount = 0;
    uint64_t pathFailCount = 0;
    uint64_t pathTimeoutCount = 0;
    llarp_time_t lastUpdated = 0s;
    llarp_time_t lastDecay = 0s;
    uint64_t version = llarp::constants::proto_version;

    RouterProfile() = default;
    RouterProfile(oxenc::bt_dict_consumer dict);

    void
    BEncode(oxenc::bt_dict_producer& dict) const;
    void
    BEncode(oxenc::bt_dict_producer&& dict) const
    {
      BEncode(dict);
    }

    void
    BDecode(oxenc::bt_dict_consumer dict);

    bool
    IsGood(uint64_t chances) const;

    bool
    IsGoodForConnect(uint64_t chances) const;

    bool
    IsGoodForPath(uint64_t chances) const;

    /// decay stats
    void
    Decay();

    // rotate stats if timeout reached
    void
    Tick();
  };

  struct Profiling
  {
    Profiling();

    inline static const int profiling_chances = 4;

    /// generic variant
    bool
    IsBad(const RouterID& r, uint64_t chances = profiling_chances) EXCLUDES(m_ProfilesMutex);

    /// check if this router should have paths built over it
    bool
    IsBadForPath(const RouterID& r, uint64_t chances = profiling_chances) EXCLUDES(m_ProfilesMutex);

    /// check if this router should be connected directly to
    bool
    IsBadForConnect(const RouterID& r, uint64_t chances = profiling_chances)
        EXCLUDES(m_ProfilesMutex);

    void
    MarkConnectTimeout(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    void
    MarkConnectSuccess(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    void
    MarkPathTimeout(path::Path* p) EXCLUDES(m_ProfilesMutex);

    void
    MarkPathFail(path::Path* p) EXCLUDES(m_ProfilesMutex);

    void
    MarkPathSuccess(path::Path* p) EXCLUDES(m_ProfilesMutex);

    void
    MarkHopFail(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    void
    ClearProfile(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    void
    Tick() EXCLUDES(m_ProfilesMutex);

    bool
    Load(const fs::path fname) EXCLUDES(m_ProfilesMutex);

    bool
    Save(const fs::path fname) EXCLUDES(m_ProfilesMutex);

    bool
    ShouldSave(llarp_time_t now) const;

    void
    Disable();

    void
    Enable();

   private:
    void
    BEncode(oxenc::bt_dict_producer& dict) const;

    void
    BDecode(oxenc::bt_dict_consumer dict);

    mutable util::Mutex m_ProfilesMutex;  // protects m_Profiles
    std::map<RouterID, RouterProfile> m_Profiles GUARDED_BY(m_ProfilesMutex);
    llarp_time_t m_LastSave = 0s;
    std::atomic<bool> m_DisableProfiling;
  };

}  // namespace llarp

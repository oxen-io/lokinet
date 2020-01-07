#ifndef LLARP_PROFILING_HPP
#define LLARP_PROFILING_HPP

#include <path/path.hpp>
#include <router_id.hpp>
#include <util/bencode.hpp>
#include <util/thread/threading.hpp>

#include <absl/base/thread_annotations.h>
#include <map>

namespace llarp
{
  struct RouterProfile
  {
    static constexpr size_t MaxSize = 256;
    uint64_t connectTimeoutCount    = 0;
    uint64_t connectGoodCount       = 0;
    uint64_t pathSuccessCount       = 0;
    uint64_t pathFailCount          = 0;
    llarp_time_t lastUpdated        = 0;
    llarp_time_t lastDecay          = 0;
    uint64_t version                = LLARP_PROTO_VERSION;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

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

    /// generic variant
    bool
    IsBad(const RouterID& r, uint64_t chances = 8)
        LOCKS_EXCLUDED(m_ProfilesMutex);

    /// check if this router should have paths built over it
    bool
    IsBadForPath(const RouterID& r, uint64_t chances = 8)
        LOCK_RETURNED(m_ProfilesMutex);

    /// check if this router should be connected directly to
    bool
    IsBadForConnect(const RouterID& r, uint64_t chances = 8)
        LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    MarkConnectTimeout(const RouterID& r) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    MarkConnectSuccess(const RouterID& r) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    MarkPathFail(path::Path* p) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    MarkPathSuccess(path::Path* p) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    MarkHopFail(const RouterID& r) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    ClearProfile(const RouterID& r) LOCKS_EXCLUDED(m_ProfilesMutex);

    void
    Tick() LOCKS_EXCLUDED(m_ProfilesMutex);

    bool
    BEncode(llarp_buffer_t* buf) const LOCKS_EXCLUDED(m_ProfilesMutex);

    bool
    DecodeKey(const llarp_buffer_t& k,
              llarp_buffer_t* buf) NO_THREAD_SAFETY_ANALYSIS;
    // disabled because we do load -> bencode::BDecodeReadFromFile -> DecodeKey

    bool
    Load(const char* fname) LOCKS_EXCLUDED(m_ProfilesMutex);

    bool
    Save(const char* fname) LOCKS_EXCLUDED(m_ProfilesMutex);

    bool
    ShouldSave(llarp_time_t now) const;

    void
    Disable();

    void
    Enable();

   private:
    bool
    BEncodeNoLock(llarp_buffer_t* buf) const
        SHARED_LOCKS_REQUIRED(m_ProfilesMutex);
    using lock_t = util::Lock;
    mutable util::Mutex m_ProfilesMutex;  // protects m_Profiles
    std::map< RouterID, RouterProfile > m_Profiles GUARDED_BY(m_ProfilesMutex);
    llarp_time_t m_LastSave = 0;
    std::atomic< bool > m_DisableProfiling;
  };

}  // namespace llarp

#endif

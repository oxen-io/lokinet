#ifndef LLARP_PROFILING_HPP
#define LLARP_PROFILING_HPP

#include <path/path.hpp>
#include <router_id.hpp>
#include <util/bencode.hpp>
#include <util/thread/threading.hpp>

#include <util/thread/annotations.hpp>
#include <map>

namespace llarp
{
  /**
     @brief router profiling data measured by a client
  */
  struct RouterProfile
  {
    /** maximum serialized size in bytes */
    static constexpr size_t MaxSize = 256;
    /** number of times connection timed out */
    uint64_t connectTimeoutCount = 0;
    /** number of times we connected successfully */
    uint64_t connectGoodCount = 0;
    /** number of paths built succesfully over this router */
    uint64_t pathSuccessCount = 0;
    /** number of paths failed to build over this router */
    uint64_t pathFailCount = 0;
    /** last timestamp updated */
    llarp_time_t lastUpdated = 0s;
    /** last timestamp decayed stats */
    llarp_time_t lastDecay = 0s;
    /** version flag for serialization */
    uint64_t version = LLARP_PROTO_VERSION;

    /** encode to buffer as bencoded dict */
    bool
    BEncode(llarp_buffer_t* buf) const;

    /** decode dict key/value */
    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    /**
       @brief determine if this router is "good"
     */
    bool
    IsGood(uint64_t chances) const;

    /**
       @brief determine if this router is "good" for connecting to
     */
    bool
    IsGoodForConnect(uint64_t chances) const;

    /**
       @brief determinate if this router is "good" for building a path over
     */
    bool
    IsGoodForPath(uint64_t chances) const;

    /** decay stats */
    void
    Decay();

    /** rotate stats if timeout reached */
    void
    Tick();
  };

  /**
      @brief holds a router profile for every router we are using
   */
  struct Profiling
  {
    Profiling();

    /// generic variant
    bool
    IsBad(const RouterID& r, uint64_t chances = 8) EXCLUDES(m_ProfilesMutex);

    /// check if this router should have paths built over it
    bool
    IsBadForPath(const RouterID& r, uint64_t chances = 8)
        EXCLUDES(m_ProfilesMutex);

    /// check if this router should be connected directly to
    bool
    IsBadForConnect(const RouterID& r, uint64_t chances = 8)
        EXCLUDES(m_ProfilesMutex);

    /** mark router by public key that a connect timeout occured */
    void
    MarkConnectTimeout(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    /** mark router by public key that we connected successfully */
    void
    MarkConnectSuccess(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    /** mark path failed at all hops */
    void
    MarkPathFail(path::Path* p) EXCLUDES(m_ProfilesMutex);

    /** mark path build success */
    void
    MarkPathSuccess(path::Path* p) EXCLUDES(m_ProfilesMutex);

    /** mark path build failed at hop by public key */
    void
    MarkHopFail(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    /** reset profile for router by public key */
    void
    ClearProfile(const RouterID& r) EXCLUDES(m_ProfilesMutex);

    /** tick and decay all profiles */
    void
    Tick() EXCLUDES(m_ProfilesMutex);

    /** encode profiles to buffer */
    bool
    BEncode(llarp_buffer_t* buf) const EXCLUDES(m_ProfilesMutex);

    /** decode router profile key/value */
    bool
    DecodeKey(const llarp_buffer_t& k,
              llarp_buffer_t* buf) NO_THREAD_SAFETY_ANALYSIS;

    /**
        disabled because we do load -> bencode::BDecodeReadFromFile -> DecodeKey
     */
    bool
    Load(const char* fname) EXCLUDES(m_ProfilesMutex);

    /**
        @brief save to file by filename
    */
    bool
    Save(const char* fname) EXCLUDES(m_ProfilesMutex);

    /**
       @brief given timestamp determine if we should save this profile async to
       disk
     */
    bool
    ShouldSave(llarp_time_t now) const;

    /**
       explicitly disable router profiling
     */
    void
    Disable();

    /**
       explicitly enable router profiling
     */
    void
    Enable();

   private:
    bool
    BEncodeNoLock(llarp_buffer_t* buf) const REQUIRES_SHARED(m_ProfilesMutex);
    mutable util::Mutex m_ProfilesMutex;  // protects m_Profiles
    std::map< RouterID, RouterProfile > m_Profiles GUARDED_BY(m_ProfilesMutex);
    llarp_time_t m_LastSave = 0s;
    std::atomic< bool > m_DisableProfiling;
  };

}  // namespace llarp

#endif

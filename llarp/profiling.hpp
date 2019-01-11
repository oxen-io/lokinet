#ifndef LLARP_PROFILING_HPP
#define LLARP_PROFILING_HPP

#include <path/path.hpp>
#include <router_id.hpp>
#include <util/bencode.hpp>
#include <util/threading.hpp>

#include <map>

namespace llarp
{
  struct RouterProfile final : public IBEncodeMessage
  {
    static constexpr size_t MaxSize = 256;
    uint64_t connectTimeoutCount    = 0;
    uint64_t connectGoodCount       = 0;
    uint64_t pathSuccessCount       = 0;
    uint64_t pathFailCount          = 0;

    RouterProfile() : IBEncodeMessage(){};

    ~RouterProfile(){};

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf) override;

    bool
    IsGood(uint64_t chances) const;
  };

  struct Profiling final : public IBEncodeMessage
  {
    Profiling() : IBEncodeMessage()
    {
    }

    ~Profiling()
    {
    }

    bool
    IsBad(const RouterID& r, uint64_t chances = 2);

    void
    MarkSuccess(const RouterID& r);

    void
    MarkTimeout(const RouterID& r);

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf) override;

    bool
    Load(const char* fname);

    bool
    Save(const char* fname);

    void
    MarkPathFail(path::Path* p);

    void
    MarkPathSuccess(path::Path* p);

   private:
    using lock_t = llarp::util::Lock;
    using mtx_t  = llarp::util::Mutex;
    mtx_t m_ProfilesMutex;
    std::map< RouterID, RouterProfile > m_Profiles;
  };

}  // namespace llarp

#endif

#include <profiling.hpp>

#include <fstream>

namespace llarp
{
  bool
  RouterProfile::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!BEncodeWriteDictInt("g", connectGoodCount, buf))
      return false;
    if(!BEncodeWriteDictInt("p", pathSuccessCount, buf))
      return false;
    if(!BEncodeWriteDictInt("s", pathFailCount, buf))
      return false;
    if(!BEncodeWriteDictInt("t", connectTimeoutCount, buf))
      return false;
    if(!BEncodeWriteDictInt("u", lastUpdated, buf))
      return false;
    if(!BEncodeWriteDictInt("v", version, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  RouterProfile::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if(!BEncodeMaybeReadDictInt("g", connectGoodCount, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("t", connectTimeoutCount, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("u", lastUpdated, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("v", version, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("s", pathFailCount, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("p", pathSuccessCount, read, k, buf))
      return false;
    return read;
  }

  void
  RouterProfile::Decay()
  {
    connectGoodCount /= 2;
    connectTimeoutCount /= 2;
    pathSuccessCount /= 2;
    pathFailCount /= 2;
    lastUpdated = llarp::time_now_ms();
  }

  void
  RouterProfile::Tick()
  {
    // 20 minutes
    static constexpr llarp_time_t updateInterval = DEFAULT_PATH_LIFETIME * 2;
    auto now                                     = llarp::time_now_ms();
    if(lastUpdated < now && now - lastUpdated > updateInterval)
    {
      Decay();
    }
  }

  bool
  RouterProfile::IsGood(uint64_t chances) const
  {
    return connectTimeoutCount <= connectGoodCount
        /// N chances
        && (pathSuccessCount * chances) >= pathFailCount;
  }

  bool
  Profiling::IsBad(const RouterID& r, uint64_t chances)
  {
    lock_t lock(m_ProfilesMutex);
    auto itr = m_Profiles.find(r);
    if(itr == m_Profiles.end())
      return false;
    return !itr->second.IsGood(chances);
  }

  void
  Profiling::Tick()
  {
    lock_t lock(m_ProfilesMutex);
    std::for_each(m_Profiles.begin(), m_Profiles.end(),
                  [](auto& item) { item.second.Tick(); });
  }

  void
  Profiling::MarkTimeout(const RouterID& r)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles[r].connectTimeoutCount += 1;
    m_Profiles[r].lastUpdated = llarp::time_now_ms();
  }

  void
  Profiling::MarkSuccess(const RouterID& r)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles[r].connectGoodCount += 1;
    m_Profiles[r].lastUpdated = llarp::time_now_ms();
  }

  void
  Profiling::MarkPathFail(path::Path* p)
  {
    lock_t lock(m_ProfilesMutex);
    for(const auto& hop : p->hops)
    {
      // TODO: also mark bad?
      m_Profiles[hop.rc.pubkey].pathFailCount += 1;
      m_Profiles[hop.rc.pubkey].lastUpdated = llarp::time_now_ms();
    }
  }

  void
  Profiling::MarkPathSuccess(path::Path* p)
  {
    lock_t lock(m_ProfilesMutex);
    for(const auto& hop : p->hops)
    {
      m_Profiles[hop.rc.pubkey].pathSuccessCount += 1;
      m_Profiles[hop.rc.pubkey].lastUpdated = llarp::time_now_ms();
    }
  }

  bool
  Profiling::Save(const char* fname)
  {
    lock_t lock(m_ProfilesMutex);
    size_t sz = (m_Profiles.size() * (RouterProfile::MaxSize + 32 + 8)) + 8;

    std::vector< byte_t > tmp(sz, 0);
    llarp_buffer_t buf(tmp);
    auto res = BEncode(&buf);
    if(res)
    {
      buf.sz = buf.cur - buf.base;
      std::ofstream f;
      f.open(fname);
      if(f.is_open())
      {
        f.write((char*)buf.base, buf.sz);
      }
    }
    return res;
  }

  bool
  Profiling::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    auto itr = m_Profiles.begin();
    while(itr != m_Profiles.end())
    {
      if(!itr->first.BEncode(buf))
        return false;
      if(!itr->second.BEncode(buf))
        return false;
      ++itr;
    }
    return bencode_end(buf);
  }

  bool
  Profiling::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    if(k.sz != 32)
      return false;
    RouterProfile profile;
    if(!profile.BDecode(buf))
      return false;
    RouterID pk = k.base;
    return m_Profiles.emplace(pk, profile).second;
  }

  bool
  Profiling::Load(const char* fname)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles.clear();
    if(!BDecodeReadFile(fname, *this))
    {
      llarp::LogWarn("failed to load router profiles from ", fname);
      return false;
    }
    return true;
  }

}  // namespace llarp

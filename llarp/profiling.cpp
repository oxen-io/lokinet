#include <llarp/profiling.hpp>
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
    if(!BEncodeWriteDictInt("t", connectTimeoutCount, buf))
      return false;
    if(!BEncodeWriteDictInt("v", version, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  RouterProfile::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
  {
    bool read = false;
    if(!BEncodeMaybeReadDictInt("g", connectGoodCount, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("t", connectTimeoutCount, read, k, buf))
      return false;
    if(!BEncodeMaybeReadDictInt("v", version, read, k, buf))
      return false;
    return read;
  }

  bool
  RouterProfile::IsGood() const
  {
    return connectTimeoutCount <= connectGoodCount;
  }

  bool
  Profiling::IsBad(const RouterID& r)
  {
    lock_t lock(m_ProfilesMutex);
    auto itr = m_Profiles.find(r);
    if(itr == m_Profiles.end())
      return false;
    return !itr->second.IsGood();
  }

  void
  Profiling::MarkTimeout(const RouterID& r)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles[r].connectTimeoutCount += 1;
  }

  void
  Profiling::MarkSuccess(const RouterID& r)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles[r].connectGoodCount += 1;
  }

  bool
  Profiling::Save(const char* fname)
  {
    lock_t lock(m_ProfilesMutex);
    size_t sz = (m_Profiles.size() * (RouterProfile::MaxSize + 32 + 8)) + 8;

    byte_t* tmp = new byte_t[sz];
    auto buf    = llarp::InitBuffer(tmp, sz);
    auto res    = BEncode(&buf);
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
    delete[] tmp;
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
  Profiling::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
  {
    if(k.sz != 32)
      return false;
    RouterProfile profile;
    if(!profile.BDecode(buf))
      return false;
    RouterID pk = k.base;
    return m_Profiles.insert(std::make_pair(pk, profile)).second;
  }

  bool
  Profiling::Load(const char* fname)
  {
    lock_t lock(m_ProfilesMutex);
    m_Profiles.clear();
    return BDecodeReadFile(fname, *this);
  }

}  // namespace llarp

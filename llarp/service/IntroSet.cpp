#include <service/IntroSet.hpp>

#include <path.hpp>

namespace llarp
{
  namespace service
  {
    IntroSet::~IntroSet()
    {
      if(W)
        delete W;
    }

    bool
    IntroSet::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("a", A, read, key, buf))
        return false;

      if(llarp_buffer_eq(key, "i"))
      {
        return BEncodeReadList(I, buf);
      }
      if(!BEncodeMaybeReadDictEntry("k", K, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictInt("t", T, read, key, buf))
        return false;

      if(llarp_buffer_eq(key, "w"))
      {
        if(W)
          delete W;
        W = new PoW();
        return W->BDecode(buf);
      }

      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("z", Z, read, key, buf))
        return false;

      return read;
    }

    bool
    IntroSet::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("a", A, buf))
        return false;
      // start introduction list
      if(!bencode_write_bytestring(buf, "i", 1))
        return false;
      if(!BEncodeWriteList(I.begin(), I.end(), buf))
        return false;
      // end introduction list

      // pq pubkey
      if(!BEncodeWriteDictEntry("k", K, buf))
        return false;

      // topic tag
      if(topic.ToString().size())
      {
        if(!BEncodeWriteDictEntry("n", topic, buf))
          return false;
      }
      // Timestamp published
      if(!BEncodeWriteDictInt("t", T, buf))
        return false;

      // write version
      if(!BEncodeWriteDictInt("v", version, buf))
        return false;
      if(W)
      {
        if(!BEncodeWriteDictEntry("w", *W, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("z", Z, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    IntroSet::HasExpiredIntros(llarp_time_t now) const
    {
      for(const auto& i : I)
        if(now >= i.expiresAt)
          return true;
      return false;
    }

    bool
    IntroSet::IsExpired(llarp_time_t now) const
    {
      return GetNewestIntroExpiration() < now;
    }

    bool
    IntroSet::Verify(llarp::Crypto* crypto, llarp_time_t now) const
    {
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      IntroSet copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      if(!A.Verify(crypto, buf, Z))
        return false;
      // validate PoW
      if(W && !W->IsValid(crypto->shorthash, now))
        return false;
      // valid timestamps
      // add max clock skew
      now += MAX_INTROSET_TIME_DELTA;
      for(const auto& intro : I)
      {
        if(intro.expiresAt > now
           && intro.expiresAt - now > DEFAULT_PATH_LIFETIME)
        {
          if(W && intro.expiresAt - W->extendedLifetime > DEFAULT_PATH_LIFETIME)
            return false;
          else if(W == nullptr)
          {
            llarp::LogWarn("intro has too high expire time");
            return false;
          }
        }
      }
      if(IsExpired(now))
      {
        llarp::LogWarn("introset expired: ", *this);
        return false;
      }
      return true;
    }

    llarp_time_t
    IntroSet::GetNewestIntroExpiration() const
    {
      llarp_time_t t = 0;
      for(const auto& intro : I)
        t = std::max(intro.expiresAt, t);
      return t;
    }
  }  // namespace service
}  // namespace llarp

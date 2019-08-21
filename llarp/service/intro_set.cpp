#include <service/intro_set.hpp>

#include <path/path.hpp>

namespace llarp
{
  namespace service
  {
    util::StatusObject
    IntroSet::ExtractStatus() const
    {
      util::StatusObject obj{{"published", T}};
      std::vector< util::StatusObject > introsObjs;
      std::transform(I.begin(), I.end(), std::back_inserter(introsObjs),
                     [](const auto& intro) -> util::StatusObject {
                       return intro.ExtractStatus();
                     });
      obj["intros"] = introsObjs;
      if(!topic.IsZero())
        obj["topic"] = topic.ToString();
      return obj;
    }

    bool
    IntroSet::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("a", A, read, key, buf))
        return false;

      if(key == "i")
      {
        return BEncodeReadList(I, buf);
      }
      if(!BEncodeMaybeReadDictEntry("k", K, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictInt("t", T, read, key, buf))
        return false;

      if(key == "w")
      {
        W = absl::make_optional< PoW >();
        return bencode_decode_dict(*W, buf);
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
    IntroSet::Verify(llarp_time_t now) const
    {
      std::array< byte_t, MAX_INTROSET_SIZE > tmp;
      llarp_buffer_t buf(tmp);
      IntroSet copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
      {
        return false;
      }
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      if(!A.Verify(buf, Z))
      {
        return false;
      }
      // validate PoW
      if(W && !W->IsValid(now))
      {
        return false;
      }
      // valid timestamps
      // add max clock skew
      now += MAX_INTROSET_TIME_DELTA;
      for(const auto& intro : I)
      {
        if(intro.expiresAt > now
           && intro.expiresAt - now > path::default_lifetime)
        {
          if(W
             && intro.expiresAt - W->extendedLifetime > path::default_lifetime)
          {
            return false;
          }
          if(!W.has_value())
          {
            LogWarn("intro has too high expire time");
            return false;
          }
        }
      }
      if(IsExpired(now))
      {
        LogWarn("introset expired: ", *this);
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

    std::ostream&
    IntroSet::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("A", A);
      printer.printAttribute("I", I);
      printer.printAttribute("K", K);

      std::string _topic = topic.ToString();

      if(!_topic.empty())
      {
        printer.printAttribute("topic", _topic);
      }
      else
      {
        printer.printAttribute("topic", topic);
      }

      printer.printAttribute("T", T);
      if(W)
      {
        printer.printAttribute("W", W.value());
      }
      else
      {
        printer.printAttribute("W", "NULL");
      }
      printer.printAttribute("V", version);
      printer.printAttribute("Z", Z);

      return stream;
    }
  }  // namespace service
}  // namespace llarp

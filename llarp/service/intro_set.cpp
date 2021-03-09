#include "intro_set.hpp"
#include <llarp/crypto/crypto.hpp>
#include <llarp/path/path.hpp>

#include <oxenmq/bt_serialize.h>

namespace llarp::service
{
  util::StatusObject
  EncryptedIntroSet::ExtractStatus() const
  {
    const auto sz = introsetPayload.size();
    return {
        {"location", derivedSigningKey.ToString()}, {"signedAt", to_json(signedAt)}, {"size", sz}};
  }

  bool
  EncryptedIntroSet::BEncode(llarp_buffer_t* buf) const
  {
    if (not bencode_start_dict(buf))
      return false;
    if (not BEncodeWriteDictEntry("d", derivedSigningKey, buf))
      return false;
    if (not BEncodeWriteDictEntry("n", nounce, buf))
      return false;
    if (not BEncodeWriteDictInt("s", signedAt.count(), buf))
      return false;
    if (not bencode_write_bytestring(buf, "x", 1))
      return false;
    if (not bencode_write_bytestring(buf, introsetPayload.data(), introsetPayload.size()))
      return false;
    if (not BEncodeWriteDictEntry("z", sig, buf))
      return false;
    return bencode_end(buf);
  }

  bool
  EncryptedIntroSet::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (key == "x")
    {
      llarp_buffer_t strbuf;
      if (not bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz > MAX_INTROSET_SIZE)
        return false;
      introsetPayload.resize(strbuf.sz);
      std::copy_n(strbuf.base, strbuf.sz, introsetPayload.data());
      return true;
    }
    if (not BEncodeMaybeReadDictEntry("d", derivedSigningKey, read, key, buf))
      return false;

    if (not BEncodeMaybeReadDictEntry("n", nounce, read, key, buf))
      return false;

    if (not BEncodeMaybeReadDictInt("s", signedAt, read, key, buf))
      return false;

    if (not BEncodeMaybeReadDictEntry("z", sig, read, key, buf))
      return false;
    return read;
  }

  bool
  EncryptedIntroSet::OtherIsNewer(const EncryptedIntroSet& other) const
  {
    return signedAt < other.signedAt;
  }

  std::ostream&
  EncryptedIntroSet::print(std::ostream& out, int levels, int spaces) const
  {
    Printer printer(out, levels, spaces);
    printer.printAttribute("d", derivedSigningKey);
    printer.printAttribute("n", nounce);
    printer.printAttribute("s", signedAt.count());
    printer.printAttribute("x", "[" + std::to_string(introsetPayload.size()) + " bytes]");
    printer.printAttribute("z", sig);
    return out;
  }

  std::optional<IntroSet>
  EncryptedIntroSet::MaybeDecrypt(const PubKey& root) const
  {
    SharedSecret k(root);
    IntroSet i;
    std::vector<byte_t> payload = introsetPayload;
    llarp_buffer_t buf(payload);
    CryptoManager::instance()->xchacha20(buf, k, nounce);
    if (not i.BDecode(&buf))
      return {};
    return i;
  }

  bool
  EncryptedIntroSet::IsExpired(llarp_time_t now) const
  {
    return now >= signedAt + path::default_lifetime;
  }

  bool
  EncryptedIntroSet::Sign(const PrivateKey& k)
  {
    signedAt = llarp::time_now_ms();
    if (not k.toPublic(derivedSigningKey))
      return false;
    sig.Zero();
    std::array<byte_t, MAX_INTROSET_SIZE + 128> tmp;
    llarp_buffer_t buf(tmp);
    if (not BEncode(&buf))
      return false;
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    if (not CryptoManager::instance()->sign(sig, k, buf))
      return false;
    LogDebug("signed encrypted introset: ", *this);
    return true;
  }

  bool
  EncryptedIntroSet::Verify(llarp_time_t now) const
  {
    if (IsExpired(now))
      return false;
    std::array<byte_t, MAX_INTROSET_SIZE + 128> tmp;
    llarp_buffer_t buf(tmp);
    EncryptedIntroSet copy(*this);
    copy.sig.Zero();
    if (not copy.BEncode(&buf))
      return false;
    LogDebug("verify encrypted introset: ", copy, " sig = ", sig);
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    return CryptoManager::instance()->verify(derivedSigningKey, buf, sig);
  }

  util::StatusObject
  IntroSet::ExtractStatus() const
  {
    util::StatusObject obj{{"published", to_json(T)}};
    std::vector<util::StatusObject> introsObjs;
    std::transform(
        I.begin(),
        I.end(),
        std::back_inserter(introsObjs),
        [](const auto& intro) -> util::StatusObject { return intro.ExtractStatus(); });
    obj["intros"] = introsObjs;
    if (!topic.IsZero())
      obj["topic"] = topic.ToString();
    return obj;
  }

  bool
  IntroSet::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("a", A, read, key, buf))
      return false;

    if (key == "i")
    {
      return BEncodeReadList(I, buf);
    }
    if (!BEncodeMaybeReadDictEntry("k", K, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
      return false;

    if (key == "s")
    {
      byte_t* begin = buf->cur;
      if (not bencode_discard(buf))
        return false;

      byte_t* end = buf->cur;

      std::string_view srvString(reinterpret_cast<char*>(begin), end - begin);

      try
      {
        oxenmq::bt_deserialize(srvString, SRVs);
      }
      catch (const oxenmq::bt_deserialize_invalid& err)
      {
        LogError("Error decoding SRV records from IntroSet: ", err.what());
        return false;
      }
      read = true;
    }

    if (!BEncodeMaybeReadDictInt("t", T, read, key, buf))
      return false;

    if (key == "w")
    {
      W.emplace();
      return bencode_decode_dict(*W, buf);
    }

    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("z", Z, read, key, buf))
      return false;

    if (read)
      return true;

    return bencode_discard(buf);
  }

  bool
  IntroSet::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;
    if (!BEncodeWriteDictEntry("a", A, buf))
      return false;
    // start introduction list
    if (!bencode_write_bytestring(buf, "i", 1))
      return false;
    if (!BEncodeWriteList(I.begin(), I.end(), buf))
      return false;
    // end introduction list

    // pq pubkey
    if (!BEncodeWriteDictEntry("k", K, buf))
      return false;

    // topic tag
    if (topic.ToString().size())
    {
      if (!BEncodeWriteDictEntry("n", topic, buf))
        return false;
    }

    if (SRVs.size())
    {
      std::string serial = oxenmq::bt_serialize(SRVs);
      if (!bencode_write_bytestring(buf, "s", 1))
        return false;
      if (!buf->write(serial.begin(), serial.end()))
        return false;
    }

    // Timestamp published
    if (!BEncodeWriteDictInt("t", T.count(), buf))
      return false;

    // write version
    if (!BEncodeWriteDictInt("v", version, buf))
      return false;
    if (W)
    {
      if (!BEncodeWriteDictEntry("w", *W, buf))
        return false;
    }
    if (!BEncodeWriteDictEntry("z", Z, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  IntroSet::HasExpiredIntros(llarp_time_t now) const
  {
    for (const auto& i : I)
      if (now >= i.expiresAt)
        return true;
    return false;
  }

  bool
  IntroSet::IsExpired(llarp_time_t now) const
  {
    return GetNewestIntroExpiration() < now;
  }

  std::vector<llarp::dns::SRVData>
  IntroSet::GetMatchingSRVRecords(std::string_view service_proto) const
  {
    std::vector<llarp::dns::SRVData> records;

    for (const auto& tuple : SRVs)
    {
      if (std::get<0>(tuple) == service_proto)
      {
        records.push_back(llarp::dns::SRVData::fromTuple(tuple));
      }
    }

    return records;
  }

  bool
  IntroSet::Verify(llarp_time_t now) const
  {
    std::array<byte_t, MAX_INTROSET_SIZE> tmp;
    llarp_buffer_t buf(tmp);
    IntroSet copy;
    copy = *this;
    copy.Z.Zero();
    if (!copy.BEncode(&buf))
    {
      return false;
    }
    // rewind and resize buffer
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    if (!A.Verify(buf, Z))
    {
      return false;
    }
    // validate PoW
    if (W && !W->IsValid(now))
    {
      return false;
    }
    // valid timestamps
    // add max clock skew
    now += MAX_INTROSET_TIME_DELTA;
    for (const auto& intro : I)
    {
      if (intro.expiresAt > now && intro.expiresAt - now > path::default_lifetime)
      {
        if (!W)
        {
          LogWarn("intro has too high expire time");
          return false;
        }
        if (intro.expiresAt - W->extendedLifetime > path::default_lifetime)
        {
          return false;
        }
      }
    }
    if (IsExpired(now))
    {
      LogWarn("introset expired: ", *this);
      return false;
    }
    return true;
  }

  llarp_time_t
  IntroSet::GetNewestIntroExpiration() const
  {
    llarp_time_t t = 0s;
    for (const auto& intro : I)
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

    if (!_topic.empty())
    {
      printer.printAttribute("topic", _topic);
    }
    else
    {
      printer.printAttribute("topic", topic);
    }

    printer.printAttribute("T", T.count());
    if (W)
    {
      printer.printAttribute("W", *W);
    }
    else
    {
      printer.printAttribute("W", "NULL");
    }
    printer.printAttribute("V", version);
    printer.printAttribute("Z", Z);

    return stream;
  }
}  // namespace llarp::service

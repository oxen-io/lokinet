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
    util::StatusObject obj{{"published", to_json(timestampSignedAt)}};
    std::vector<util::StatusObject> introsObjs;
    std::transform(
        intros.begin(),
        intros.end(),
        std::back_inserter(introsObjs),
        [](const auto& intro) -> util::StatusObject { return intro.ExtractStatus(); });
    obj["intros"] = introsObjs;
    if (!topic.IsZero())
      obj["topic"] = topic.ToString();

    std::vector<util::StatusObject> protocols;
    std::transform(
        supportedProtocols.begin(),
        supportedProtocols.end(),
        std::back_inserter(protocols),
        [](const auto& proto) -> util::StatusObject {
          std::stringstream ss;
          ss << proto;
          return ss.str();
        });
    obj["protos"] = protocols;
    std::vector<util::StatusObject> ranges;
    std::transform(
        ownedRanges.begin(),
        ownedRanges.end(),
        std::back_inserter(ranges),
        [](const auto& range) -> util::StatusObject { return range.ToString(); });

    obj["advertisedRanges"] = ranges;
    if (exitTrafficPolicy)
      obj["exitPolicy"] = exitTrafficPolicy->ExtractStatus();

    return obj;
  }

  bool
  IntroSet::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("a", addressKeys, read, key, buf))
      return false;

    if (key == "e")
    {
      net::TrafficPolicy policy;
      if (not policy.BDecode(buf))
        return false;
      exitTrafficPolicy = policy;
      return true;
    }

    if (key == "i")
    {
      return BEncodeReadList(intros, buf);
    }
    if (!BEncodeMaybeReadDictEntry("k", sntrupKey, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
      return false;

    if (key == "p")
    {
      return bencode_read_list(
          [&](llarp_buffer_t* buf, bool more) {
            if (more)
            {
              uint64_t protoval;
              if (not bencode_read_integer(buf, &protoval))
                return false;
              supportedProtocols.emplace_back(static_cast<ProtocolType>(protoval));
            }
            return true;
          },
          buf);
    }

    if (key == "r")
    {
      return BEncodeReadSet(ownedRanges, buf);
    }

    if (key == "s")
    {
      byte_t* begin = buf->cur;
      if (not bencode_discard(buf))
        return false;

      byte_t* end = buf->cur;

      std::string_view srvString(
          reinterpret_cast<const char*>(begin), static_cast<size_t>(end - begin));

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

    if (!BEncodeMaybeReadDictInt("t", timestampSignedAt, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
      return false;

    return read or bencode_discard(buf);
  }

  bool
  IntroSet::BEncode(llarp_buffer_t* buf) const
  {
    if (not bencode_start_dict(buf))
      return false;
    if (not BEncodeWriteDictEntry("a", addressKeys, buf))
      return false;

    // exit policy if applicable
    if (exitTrafficPolicy)
    {
      if (not BEncodeWriteDictEntry("e", *exitTrafficPolicy, buf))
        return false;
    }
    // start introduction list
    if (not bencode_write_bytestring(buf, "i", 1))
      return false;
    if (not BEncodeWriteList(intros.begin(), intros.end(), buf))
      return false;
    // end introduction list

    // pq pubkey
    if (not BEncodeWriteDictEntry("k", sntrupKey, buf))
      return false;

    // topic tag
    if (not topic.ToString().empty())
    {
      if (not BEncodeWriteDictEntry("n", topic, buf))
        return false;
    }

    // supported ethertypes
    if (not supportedProtocols.empty())
    {
      if (not bencode_write_bytestring(buf, "p", 1))
        return false;

      if (not bencode_start_list(buf))
        return false;

      for (const auto& proto : supportedProtocols)
      {
        if (not bencode_write_uint64(buf, static_cast<uint64_t>(proto)))
          return false;
      }

      if (not bencode_end(buf))
        return false;
    }

    // owned ranges
    if (not ownedRanges.empty())
    {
      if (not bencode_write_bytestring(buf, "r", 1))
        return false;

      if (not BEncodeWriteSet(ownedRanges, buf))
        return false;
    }

    // srv records
    if (not SRVs.empty())
    {
      std::string serial = oxenmq::bt_serialize(SRVs);
      if (!bencode_write_bytestring(buf, "s", 1))
        return false;
      if (!buf->write(serial.begin(), serial.end()))
        return false;
    }

    // timestamp
    if (!BEncodeWriteDictInt("t", timestampSignedAt.count(), buf))
      return false;

    // write version
    if (!BEncodeWriteDictInt("v", version, buf))
      return false;

    if (!BEncodeWriteDictEntry("z", signature, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  IntroSet::HasExpiredIntros(llarp_time_t now) const
  {
    for (const auto& intro : intros)
      if (now >= intro.expiresAt)
        return true;
    return false;
  }

  bool
  IntroSet::HasStaleIntros(llarp_time_t now, llarp_time_t delta) const
  {
    for (const auto& intro : intros)
      if (intro.ExpiresSoon(now, delta))
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
    llarp_buffer_t buf{tmp};
    IntroSet copy;
    copy = *this;
    copy.signature.Zero();
    if (!copy.BEncode(&buf))
    {
      return false;
    }
    // rewind and resize buffer
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    if (!addressKeys.Verify(buf, signature))
    {
      return false;
    }
    // valid timestamps
    // add max clock skew
    now += MAX_INTROSET_TIME_DELTA;
    for (const auto& intro : intros)
    {
      if (intro.expiresAt > now && intro.expiresAt - now > path::default_lifetime)
      {
        return false;
      }
    }
    return not IsExpired(now);
  }

  llarp_time_t
  IntroSet::GetNewestIntroExpiration() const
  {
    llarp_time_t maxTime = 0s;
    for (const auto& intro : intros)
      maxTime = std::max(intro.expiresAt, maxTime);
    return maxTime;
  }

  std::ostream&
  IntroSet::print(std::ostream& stream, int level, int spaces) const
  {
    Printer printer(stream, level, spaces);
    printer.printAttribute("addressKeys", addressKeys);
    printer.printAttribute("intros", intros);
    printer.printAttribute("sntrupKey", sntrupKey);

    std::string _topic = topic.ToString();

    if (!_topic.empty())
    {
      printer.printAttribute("topic", _topic);
    }
    else
    {
      printer.printAttribute("topic", topic);
    }

    printer.printAttribute("signedAt", timestampSignedAt.count());

    printer.printAttribute("version", version);
    printer.printAttribute("sig", signature);

    return stream;
  }
}  // namespace llarp::service

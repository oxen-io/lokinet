#include "intro_set.hpp"
#include <llarp/crypto/crypto.hpp>
#include <llarp/path/path.hpp>

#include <oxenc/bt_serialize.h>

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
  EncryptedIntroSet::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (key.startswith("x"))
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

  std::string
  EncryptedIntroSet::ToString() const
  {
    return fmt::format(
        "[EncIntroSet d={} n={} s={} x=[{} bytes] z={}]",
        derivedSigningKey,
        nounce,
        signedAt.count(),
        introsetPayload.size(),
        sig);
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
    util::StatusObject obj{{"published", to_json(time_signed)}};
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
        supported_protocols.begin(),
        supported_protocols.end(),
        std::back_inserter(protocols),
        [](const auto& proto) -> util::StatusObject { return service::ToString(proto); });
    obj["protos"] = protocols;
    std::vector<util::StatusObject> ranges;
    std::transform(
        owned_ranges.begin(),
        owned_ranges.end(),
        std::back_inserter(ranges),
        [](const auto& range) -> util::StatusObject { return range.ToString(); });

    obj["advertisedRanges"] = ranges;
    if (exit_policy)
      obj["exitPolicy"] = exit_policy->ExtractStatus();

    return obj;
  }

  bool
  IntroSet::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("a", addressKeys, read, key, buf))
      return false;

    if (key.startswith("e"))
    {
      net::TrafficPolicy policy;
      if (not policy.BDecode(buf))
        return false;
      exit_policy = policy;
      return true;
    }

    if (key.startswith("i"))
    {
      return BEncodeReadList(intros, buf);
    }
    if (!BEncodeMaybeReadDictEntry("k", sntru_pubkey, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
      return false;

    if (key.startswith("p"))
    {
      return bencode_read_list(
          [&](llarp_buffer_t* buf, bool more) {
            if (more)
            {
              uint64_t protoval;
              if (not bencode_read_integer(buf, &protoval))
                return false;
              supported_protocols.emplace_back(static_cast<ProtocolType>(protoval));
            }
            return true;
          },
          buf);
    }

    if (key.startswith("r"))
    {
      return BEncodeReadSet(owned_ranges, buf);
    }

    if (key.startswith("s"))
    {
      byte_t* begin = buf->cur;
      if (not bencode_discard(buf))
        return false;

      byte_t* end = buf->cur;

      std::string_view srvString(
          reinterpret_cast<const char*>(begin), static_cast<size_t>(end - begin));

      try
      {
        oxenc::bt_deserialize(srvString, SRVs);
      }
      catch (const oxenc::bt_deserialize_invalid& err)
      {
        LogError("Error decoding SRV records from IntroSet: ", err.what());
        return false;
      }
      read = true;
    }

    if (!BEncodeMaybeReadDictInt("t", time_signed, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
      return false;

    return read or bencode_discard(buf);
  }

  std::string
  IntroSet::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      {
        auto subdict = btdp.append_dict("a");
        addressKeys.bt_encode(subdict);
      }

      if (exit_policy)
      {
        auto subdict = btdp.append_dict("e");
        exit_policy->bt_encode(subdict);
      }

      {
        auto subdict = btdp.append_dict("i");
        for (auto& i : intros)
          i.bt_encode(subdict);
      }

      btdp.append("k", sntru_pubkey.ToView());
      btdp.append("n", topic.ToView());

      if (not supported_protocols.empty())
      {
        auto sublist = btdp.append_list("p");
        for (auto& p : supported_protocols)
          sublist.append(static_cast<uint64_t>(p));
      }

      if (not owned_ranges.empty())
      {
        auto sublist = btdp.append_list("s");
        for (auto& r : owned_ranges)
          r.bt_encode(sublist);
      }

      if (not SRVs.empty())
        btdp.append("s", oxenc::bt_serialize(SRVs));

      btdp.append("t", time_signed.count());
      btdp.append("v", version);
      btdp.append("z", signature.ToView());
    }
    catch (...)
    {
      log::critical(net_cat, "Error: IntroSet failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  IntroSet::HasExpiredIntros(llarp_time_t now) const
  {
    for (const auto& intro : intros)
      if (now >= intro.expiry)
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

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

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
      if (intro.expiry > now && intro.expiry - now > path::default_lifetime)
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
      maxTime = std::max(intro.expiry, maxTime);
    return maxTime;
  }

  std::string
  IntroSet::ToString() const
  {
    return fmt::format(
        "[IntroSet addressKeys={} intros={{{}}} sntrupKey={} topic={} signedAt={} v={} sig={}]",
        addressKeys,
        fmt::format("{}", fmt::join(intros, ",")),
        sntru_pubkey,
        topic,
        time_signed.count(),
        version,
        signature);
  }
}  // namespace llarp::service

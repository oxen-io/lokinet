#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/pow.hpp>
#include "info.hpp"
#include "intro.hpp"
#include "tag.hpp"
#include "protocol_type.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/status.hpp>
#include <llarp/dns/srv_data.hpp>

#include <llarp/net/ip_range.hpp>
#include <llarp/net/traffic_policy.hpp>

#include <optional>
#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

namespace llarp::service
{
  constexpr std::size_t MAX_INTROSET_SIZE = 4096;
  // 10 seconds clock skew permitted for introset expiration
  constexpr llarp_time_t MAX_INTROSET_TIME_DELTA = 10s;

  struct IntroSet
  {
    ServiceInfo addressKeys;
    std::vector<Introduction> intros;
    PQPubKey sntru_pubkey;
    Tag topic;
    std::vector<llarp::dns::SRVTuple> SRVs;
    llarp_time_t time_signed = 0s;

    /// ethertypes we advertise that we speak
    std::vector<ProtocolType> supported_protocols;
    /// aonnuce that these ranges are reachable via our endpoint
    /// only set when we support exit traffic ethertype is supported
    std::set<IPRange> owned_ranges;

    /// policies about traffic that we are willing to carry
    /// a protocol/range whitelist or blacklist
    /// only set when we support exit traffic ethertype
    std::optional<net::TrafficPolicy> exit_policy;

    Signature signature;
    uint64_t version = llarp::constants::proto_version;

    bool
    OtherIsNewer(const IntroSet& other) const
    {
      return time_signed < other.time_signed;
    }

    std::string
    ToString() const;

    llarp_time_t
    GetNewestIntroExpiration() const;

    bool
    GetNewestIntro(Introduction& intro) const;

    bool
    HasExpiredIntros(llarp_time_t now) const;

    /// return true if any of our intros expires soon given a delta
    bool
    HasStaleIntros(llarp_time_t now, llarp_time_t delta) const;

    bool
    IsExpired(llarp_time_t now) const;

    std::vector<llarp::dns::SRVData>
    GetMatchingSRVRecords(std::string_view service_proto) const;

    std::string
    bt_encode() const;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf);

    bool
    Verify(llarp_time_t now) const;

    util::StatusObject
    ExtractStatus() const;
  };

  inline bool
  operator<(const IntroSet& lhs, const IntroSet& rhs)
  {
    return lhs.addressKeys < rhs.addressKeys;
  }

  inline bool
  operator==(const IntroSet& lhs, const IntroSet& rhs)
  {
    return std::tie(
               lhs.addressKeys,
               lhs.intros,
               lhs.sntru_pubkey,
               lhs.time_signed,
               lhs.version,
               lhs.topic,
               lhs.signature)
        == std::tie(
               rhs.addressKeys,
               rhs.intros,
               rhs.sntru_pubkey,
               rhs.time_signed,
               rhs.version,
               rhs.topic,
               rhs.signature);
  }

  inline bool
  operator!=(const IntroSet& lhs, const IntroSet& rhs)
  {
    return !(lhs == rhs);
  }

  /// public version of the introset that is encrypted
  struct EncryptedIntroSet
  {
    using Payload_t = std::vector<byte_t>;

    PubKey derivedSigningKey;
    llarp_time_t signedAt = 0s;
    Payload_t introsetPayload;
    TunnelNonce nounce;
    std::optional<Tag> topic;
    Signature sig;

    bool
    Sign(const PrivateKey& k);

    bool
    IsExpired(llarp_time_t now) const;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf);

    bool
    OtherIsNewer(const EncryptedIntroSet& other) const;

    /// verify signature and timestamp
    bool
    Verify(llarp_time_t now) const;

    std::string
    ToString() const;

    util::StatusObject
    ExtractStatus() const;

    std::optional<IntroSet>
    MaybeDecrypt(const PubKey& rootKey) const;
  };

  inline bool
  operator<(const EncryptedIntroSet& lhs, const EncryptedIntroSet& rhs)
  {
    return lhs.derivedSigningKey < rhs.derivedSigningKey;
  }

  inline bool
  operator==(const EncryptedIntroSet& lhs, const EncryptedIntroSet& rhs)
  {
    return std::tie(lhs.signedAt, lhs.derivedSigningKey, lhs.nounce, lhs.sig)
        == std::tie(rhs.signedAt, rhs.derivedSigningKey, rhs.nounce, rhs.sig);
  }

  inline bool
  operator!=(const EncryptedIntroSet& lhs, const EncryptedIntroSet& rhs)
  {
    return !(lhs == rhs);
  }

  using EncryptedIntroSetLookupHandler = std::function<void(const std::vector<EncryptedIntroSet>&)>;
  using IntroSetLookupHandler = std::function<void(const std::vector<IntroSet>&)>;

}  // namespace llarp::service

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::service::IntroSet> = true;
template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::service::EncryptedIntroSet> = true;

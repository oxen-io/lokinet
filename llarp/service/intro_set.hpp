#ifndef LLARP_SERVICE_INTRO_SET_HPP
#define LLARP_SERVICE_INTRO_SET_HPP

#include <crypto/types.hpp>
#include <pow.hpp>
#include <service/info.hpp>
#include <service/intro.hpp>
#include <service/tag.hpp>
#include <util/bencode.hpp>
#include <util/time.hpp>
#include <util/status.hpp>
#include <dns/srv_data.hpp>

#include <optional>
#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 4096;
    // 10 seconds clock skew permitted for introset expiration
    constexpr llarp_time_t MAX_INTROSET_TIME_DELTA = 10s;

    struct IntroSet
    {
      ServiceInfo A;
      std::vector<Introduction> I;
      PQPubKey K;
      Tag topic;
      std::vector<llarp::dns::SRVTuple> SRVs;
      llarp_time_t T = 0s;
      std::optional<PoW> W;
      Signature Z;
      uint64_t version = LLARP_PROTO_VERSION;

      bool
      OtherIsNewer(const IntroSet& other) const
      {
        return T < other.T;
      }

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      llarp_time_t
      GetNewestIntroExpiration() const;

      bool
      GetNewestIntro(Introduction& intro) const;

      bool
      HasExpiredIntros(llarp_time_t now) const;

      bool
      IsExpired(llarp_time_t now) const;

      std::vector<llarp::dns::SRVData>
      GetMatchingSRVRecords(std::string_view service_proto) const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        return bencode_decode_dict(*this, buf);
      }

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

      bool
      Verify(llarp_time_t now) const;

      util::StatusObject
      ExtractStatus() const;
    };

    inline bool
    operator<(const IntroSet& lhs, const IntroSet& rhs)
    {
      return lhs.A < rhs.A;
    }

    inline bool
    operator==(const IntroSet& lhs, const IntroSet& rhs)
    {
      return std::tie(lhs.A, lhs.I, lhs.K, lhs.T, lhs.version, lhs.topic, lhs.W, lhs.Z)
          == std::tie(rhs.A, rhs.I, rhs.K, rhs.T, rhs.version, rhs.topic, rhs.W, rhs.Z);
    }

    inline bool
    operator!=(const IntroSet& lhs, const IntroSet& rhs)
    {
      return !(lhs == rhs);
    }

    inline std::ostream&
    operator<<(std::ostream& out, const IntroSet& i)
    {
      return i.print(out, -1, -1);
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
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

      bool
      OtherIsNewer(const EncryptedIntroSet& other) const;

      /// verify signature and timestamp
      bool
      Verify(llarp_time_t now) const;

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      util::StatusObject
      ExtractStatus() const;

      std::optional<IntroSet>
      MaybeDecrypt(const PubKey& rootKey) const;
    };

    inline std::ostream&
    operator<<(std::ostream& out, const EncryptedIntroSet& i)
    {
      return i.print(out, -1, -1);
    }

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

    using EncryptedIntroSetLookupHandler =
        std::function<void(const std::vector<EncryptedIntroSet>&)>;
    using IntroSetLookupHandler = std::function<void(const std::vector<IntroSet>&)>;

  }  // namespace service
}  // namespace llarp

#endif

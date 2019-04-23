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

#include <absl/types/optional.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

namespace llarp
{
  struct Crypto;

  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 4096;
    // 10 seconds clock skew permitted for introset expiration
    constexpr llarp_time_t MAX_INTROSET_TIME_DELTA = (10 * 1000);
    struct IntroSet final : public IBEncodeMessage
    {
      ServiceInfo A;
      std::vector< Introduction > I;
      PQPubKey K;
      Tag topic;
      llarp_time_t T = 0;
      absl::optional< PoW > W;
      Signature Z;

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
      HasExpiredIntros(llarp_time_t now) const;

      bool
      IsExpired(llarp_time_t now) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      Verify(Crypto* crypto, llarp_time_t now) const;

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
      return std::tie(lhs.A, lhs.I, lhs.K, lhs.T, lhs.version, lhs.topic, lhs.W,
                      lhs.Z)
          == std::tie(rhs.A, rhs.I, rhs.K, rhs.T, rhs.version, rhs.topic, rhs.W,
                      rhs.Z);
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

    using IntroSetLookupHandler =
        std::function< void(const std::vector< IntroSet >&) >;

  }  // namespace service
}  // namespace llarp

#endif

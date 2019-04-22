#ifndef LLARP_SERVICE_INTROSET_HPP
#define LLARP_SERVICE_INTROSET_HPP

#include <crypto/types.hpp>
#include <pow.hpp>
#include <service/Info.hpp>
#include <service/Intro.hpp>
#include <service/tag.hpp>
#include <util/bencode.hpp>
#include <util/time.hpp>
#include <util/status.hpp>

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
      std::unique_ptr< PoW > W;
      Signature Z;

      IntroSet() = default;

      IntroSet(IntroSet&& other) = default;

      IntroSet(const IntroSet& other)
          : IBEncodeMessage(other.version)
          , A(other.A)
          , I(other.I)
          , K(other.K)
          , topic(other.topic)
          , T(other.T)
          , W(std::make_unique< PoW >(*other.W))
          , Z(other.Z)
      {
      }

      IntroSet&
      operator=(const IntroSet& other)
      {
        I.clear();
        A       = other.A;
        I       = other.I;
        K       = other.K;
        T       = other.T;
        version = other.version;
        topic   = other.topic;
        W.reset();
        if(other.W)
        {
          W = std::make_unique< PoW >(*other.W);
        }
        Z = other.Z;
        return *this;
      }

      bool
      operator<(const IntroSet& other) const
      {
        return A < other.A;
      }

      bool
      operator==(const IntroSet& other) const
      {
        if(std::tie(A, I, K, T, version, topic, Z)
           != std::tie(other.A, other.I, other.K, other.T, other.version,
                       other.topic, other.Z))
        {
          return false;
        }
        else if(W && other.W)  // both PoW have a valid ptr
        {
          return *W == *other.W;
        }
        else
        {
          return W == other.W;  // if one is null, verify the other is null
        }
      }

      bool
      operator!=(const IntroSet& other) const
      {
        return !(*this == other);
      }

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

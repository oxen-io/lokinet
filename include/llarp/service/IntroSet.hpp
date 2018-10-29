#ifndef LLARP_SERVICE_INTROSET_HPP
#define LLARP_SERVICE_INTROSET_HPP
#include <llarp/time.h>
#include <iostream>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/pow.hpp>
#include <llarp/service/Info.hpp>
#include <llarp/service/Intro.hpp>
#include <llarp/service/tag.hpp>

#include <vector>
#include <algorithm>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 4096;
    // 10 seconds clock skew permitted for introset expiration
    constexpr llarp_time_t MAX_INTROSET_TIME_DELTA = (10 * 1000);
    struct IntroSet : public llarp::IBEncodeMessage
    {
      ServiceInfo A;
      std::vector< Introduction > I;
      PQPubKey K;
      Tag topic;
      llarp_time_t T = 0;
      llarp::PoW* W  = nullptr;
      llarp::Signature Z;

      IntroSet() = default;

      IntroSet(IntroSet&& other)
      {
        A       = std::move(other.A);
        I       = std::move(other.I);
        K       = std::move(other.K);
        T       = std::move(other.T);
        version = std::move(other.version);
        topic   = std::move(other.topic);
        W       = std::move(other.W);
        Z       = std::move(other.Z);
      }

      IntroSet(const IntroSet& other)
      {
        A       = other.A;
        I       = other.I;
        K       = other.K;
        T       = other.T;
        version = other.version;
        topic   = other.topic;
        if(other.W)
          W = new llarp::PoW(*other.W);
        Z = other.Z;
      }

      ~IntroSet();

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
        if(W)
        {
          delete W;
          W = nullptr;
        }
        if(other.W)
          W = new llarp::PoW(*other.W);
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
        return A == other.A && I == other.I && K == other.K && T == other.T
            && version == other.version && topic == other.topic && W == other.W
            && Z == other.Z;
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

      friend std::ostream&
      operator<<(std::ostream& out, const IntroSet& i)
      {
        out << "A=[" << i.A << "] I=[";
        for(const auto& intro : i.I)
        {
          out << intro << ", ";
        }
        out << "]";
        out << "K=" << i.K;
        auto topic = i.topic.ToString();
        if(topic.size())
        {
          out << " topic=" << topic;
        }
        else
        {
          out << " topic=" << i.topic;
        }
        out << " T=" << i.T;
        if(i.W)
        {
          out << " W=" << *i.W;
        }
        return out << " V=" << i.version << " Z=" << i.Z;
      }

      llarp_time_t
      GetNewestIntroExpiration() const;

      bool
      HasExpiredIntros(llarp_time_t now) const;

      bool
      IsExpired(llarp_time_t now) const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      Verify(llarp_crypto* crypto, llarp_time_t now) const;
    };
  }  // namespace service
}  // namespace llarp

#endif

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

#include <set>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 1024;

    struct IntroSet : public llarp::IBEncodeMessage
    {
      ServiceInfo A;
      std::set< Introduction > I;
      Tag topic;
      llarp::PoW* W = nullptr;
      llarp::Signature Z;

      ~IntroSet();

      IntroSet&
      operator=(const IntroSet& other)
      {
        A       = other.A;
        I       = other.I;
        version = other.version;
        topic   = other.topic;
        if(W)
          delete W;
        W = other.W;
        Z = other.Z;
        return *this;
      }

      bool
      operator<(const IntroSet& other) const
      {
        return A < other.A;
      }

      friend std::ostream&
      operator<<(std::ostream& out, const IntroSet& i)
      {
        out << "A=[" << i.A << "] I=[";
        for(const auto& intro : i.I)
        {
          out << intro << ",";
        }
        out << "]";
        auto topic = i.topic.ToString();
        if(topic.size())
        {
          out << " topic=" << topic;
        }
        else
        {
          out << " topic=" << i.topic;
        }
        if(i.W)
        {
          out << " W=" << *i.W;
        }
        return out << " V=" << i.version << " Z=" << i.Z;
      }

      bool
      HasExpiredIntros(llarp_time_t now) const;

      bool
      IsExpired(llarp_time_t now) const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      VerifySignature(llarp_crypto* crypto) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
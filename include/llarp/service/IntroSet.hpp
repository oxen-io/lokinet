#ifndef LLARP_SERVICE_INTROSET_HPP
#define LLARP_SERVICE_INTROSET_HPP
#include <iostream>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/pow.hpp>
#include <llarp/service/Info.hpp>
#include <llarp/service/Intro.hpp>

#include <list>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 1024;

    struct IntroSet : public llarp::IBEncodeMessage
    {
      ServiceInfo A;
      std::list< Introduction > I;
      llarp::PoW* W = nullptr;
      llarp::Signature Z;

      ~IntroSet();

      IntroSet&
      operator=(const IntroSet& other)
      {
        A       = other.A;
        I       = other.I;
        version = other.version;
        if(W)
          delete W;
        W = other.W;
        Z = other.Z;
        return *this;
      }

      friend std::ostream&
      operator<<(std::ostream& out, const IntroSet& i)
      {
        out << "A=[" << i.A << "] I=[";
        for(const auto& intro : i.I)
        {
          out << intro << ",";
        }
        return out << "] V=" << i.version << " Z=" << i.Z;
      }

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
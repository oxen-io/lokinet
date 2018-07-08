#ifndef LLARP_SERVICE_HPP
#define LLARP_SERVICE_HPP
#include <llarp/aligned.hpp>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/path_types.hpp>
#include <llarp/pow.hpp>

#include <iostream>
#include <set>
#include <string>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_INTROSET_SIZE = 1024;

    // forward declare
    struct IntroSet;

    /// hidden service address
    typedef llarp::AlignedBuffer< 32 > Address;

    std::string
    AddressToString(const Address& addr);

    typedef llarp::AlignedBuffer< 16 > VanityNonce;

    struct ServiceInfo : public llarp::IBEncodeMessage
    {
      llarp::PubKey enckey;
      llarp::PubKey signkey;
      uint64_t version = 0;
      VanityNonce vanity;

      ServiceInfo();

      ~ServiceInfo();

      ServiceInfo&
      operator=(const ServiceInfo& other)
      {
        enckey  = other.enckey;
        signkey = other.signkey;
        version = other.version;
        vanity  = other.vanity;
        return *this;
      };

      friend std::ostream&
      operator<<(std::ostream& out, const ServiceInfo& i)
      {
        return out << "[e=" << i.enckey << " s=" << i.signkey
                   << " v=" << i.version << " x=" << i.vanity << "]";
      }

      /// calculate our address
      bool
      CalculateAddress(Address& addr) const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);
    };

    // private keys
    struct Identity : public llarp::IBEncodeMessage
    {
      llarp::SecretKey enckey;
      llarp::SecretKey signkey;
      uint64_t version = 0;
      VanityNonce vanity;

      // public service info
      ServiceInfo pub;

      ~Identity();

      // regenerate secret keys
      void
      RegenerateKeys(llarp_crypto* c);

      // load from file
      bool
      LoadFromFile(const std::string& fpath);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      SignIntroSet(IntroSet& i, llarp_crypto* c) const;
    };

    struct Introduction : public llarp::IBEncodeMessage
    {
      llarp::PubKey router;
      llarp::PathID_t pathID;
      uint64_t version = 0;
      uint64_t expiresAt;

      ~Introduction();

      friend std::ostream&
      operator<<(std::ostream& out, const Introduction& i)
      {
        return out << "k=" << i.router << " p=" << i.pathID
                   << " v=" << i.version << " x=" << i.expiresAt;
      }

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      operator<(const Introduction& other) const
      {
        return expiresAt < other.expiresAt || pathID < other.pathID;
      }
    };

    struct IntroSet : public llarp::IBEncodeMessage
    {
      ServiceInfo A;
      std::set< Introduction > I;
      uint64_t V    = 0;
      llarp::PoW* W = nullptr;
      llarp::Signature Z;

      ~IntroSet();

      IntroSet&
      operator=(const IntroSet& other)
      {
        A = other.A;
        I = other.I;
        V = other.V;
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
        return out << "] V=" << i.V << " Z=" << i.Z;
      }

      bool
      BDecode(llarp_buffer_t* buf);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      VerifySignature(llarp_crypto* crypto) const;
    };

  };  // namespace service
}  // namespace llarp

#endif
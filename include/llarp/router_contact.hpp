#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP
#include <llarp/address_info.hpp>
#include <llarp/crypto.h>
#include <llarp/bencode.hpp>
#include <llarp/exit_info.hpp>

#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace llarp
{
  struct RouterContact : public IBEncodeMessage
  {
    RouterContact() : IBEncodeMessage()
    {
      Clear();
    }

    RouterContact(const RouterContact &other)
        : addrs(other.addrs)
        , enckey(other.enckey)
        , pubkey(other.pubkey)
        , exits(other.exits)
        , signature(other.signature)
        , nickname(other.nickname)
        , last_updated(other.last_updated)

    {
      version = other.version;
    }

    // advertised addresses
    std::vector< AddressInfo > addrs = {};
    // public encryption public key
    llarp::PubKey enckey;
    // public signing public key
    llarp::PubKey pubkey;
    // advertised exits
    std::vector< ExitInfo > exits = {};
    // signature
    llarp::Signature signature;
    /// node nickname, yw kee
    llarp::AlignedBuffer< NICKLEN > nickname;

    uint64_t last_updated;

    bool
    BEncode(llarp_buffer_t *buf) const;

    void
    Clear();

    bool
    BDecode(llarp_buffer_t *buf)
    {
      Clear();
      return IBEncodeMessage::BDecode(buf);
    }

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t *buf);

    RouterContact &
    operator=(const RouterContact &other);

    bool
    HasNick() const;

    std::string
    Nick() const;

    bool
    IsPublicRouter() const;

    void
    SetNick(const std::string &nick);

    bool
    VerifySignature(llarp_crypto *crypto) const;

    bool
    Sign(llarp_crypto *crypto, const llarp::SecretKey &secret);

    bool
    Read(const char *fname);

    bool
    Write(const char *fname) const;
  };
}  // namespace llarp

#endif

#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP
#include <address_info.hpp>

#include <llarp/bencode.hpp>
#include <llarp/crypto.h>
#include <llarp/exit_info.hpp>

#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace llarp
{
  struct RouterContact final : public IBEncodeMessage
  {
    RouterContact() : IBEncodeMessage()
    {
      Clear();
    }

    RouterContact(const RouterContact &other)
        : IBEncodeMessage()
        , addrs(other.addrs)
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
    std::vector< AddressInfo > addrs;
    // public encryption public key
    llarp::PubKey enckey;
    // public signing public key
    llarp::PubKey pubkey;
    // advertised exits
    std::vector< ExitInfo > exits;
    // signature
    llarp::Signature signature;
    /// node nickname, yw kee
    llarp::AlignedBuffer< NICKLEN > nickname;

    uint64_t last_updated;

    bool
    BEncode(llarp_buffer_t *buf) const override;

    void
    Clear();

    bool
    IsExit() const
    {
      return exits.size() > 0;
    }

    bool
    BDecode(llarp_buffer_t *buf) override
    {
      Clear();
      return IBEncodeMessage::BDecode(buf);
    }

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t *buf) override;

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
    Verify(llarp::Crypto *crypto) const;

    bool
    Sign(llarp::Crypto *crypto, const llarp::SecretKey &secret);

    bool
    OtherIsNewer(const RouterContact &other) const
    {
      return last_updated < other.last_updated;
    }

    bool
    Read(const char *fname);

    bool
    Write(const char *fname) const;

   private:
    bool
    VerifySignature(llarp::Crypto *crypto) const;
  };
}  // namespace llarp

#endif

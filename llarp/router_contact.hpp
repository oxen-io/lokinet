#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP

#include <address_info.hpp>
#include <bencode.hpp>
#include <crypto.h>
#include <exit_info.hpp>
#include <version.hpp>

#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace llarp
{
  /// NetID
  struct NetID final : public AlignedBuffer< 8 >
  {
    static NetID &
    DefaultValue();

    NetID();

    explicit NetID(const byte_t *val);

    explicit NetID(const NetID &other) = default;

    bool
    operator==(const NetID &other) const;

    bool
    operator!=(const NetID &other) const
    {
      return !(*this == other);
    }

    friend std::ostream &
    operator<<(std::ostream &out, const NetID &id)
    {
      return out << id.ToString();
    }

    std::string
    ToString() const;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  /// RouterContact
  struct RouterContact final : public IBEncodeMessage
  {
    /// for unit tests
    static bool IgnoreBogons;

    static llarp_time_t Lifetime;

    RouterContact() : IBEncodeMessage()
    {
      Clear();
    }

    RouterContact(const RouterContact &other)
        : IBEncodeMessage(other.version)
        , addrs(other.addrs)
        , netID(other.netID)
        , enckey(other.enckey)
        , pubkey(other.pubkey)
        , exits(other.exits)
        , signature(other.signature)
        , nickname(other.nickname)
        , last_updated(other.last_updated)
    {
    }

    // advertised addresses
    std::vector< AddressInfo > addrs;
    // network identifier
    NetID netID;
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

    bool
    operator==(const RouterContact &other) const
    {
      return addrs == other.addrs && enckey == other.enckey
          && pubkey == other.pubkey && signature == other.signature
          && nickname == other.nickname && last_updated == other.last_updated
          && netID == other.netID;
    }

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
    Verify(llarp::Crypto *crypto, llarp_time_t now) const;

    bool
    Sign(llarp::Crypto *crypto, const llarp::SecretKey &secret);

    /// does this RC expire soon? default delta is 1 minute
    bool
    ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 60000) const;

    bool
    IsExpired(llarp_time_t now) const;

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

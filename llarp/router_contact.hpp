#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP

#include <constants/version.hpp>
#include <crypto/types.hpp>
#include <net/address_info.hpp>
#include <net/exit_info.hpp>
#include <util/aligned.hpp>
#include <util/bencode.hpp>
#include <util/status.hpp>

#include <functional>
#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace llarp
{
  struct Crypto;

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

    std::ostream &
    print(std::ostream &stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue(ToString());
      return stream;
    }

    std::string
    ToString() const;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  inline std::ostream &
  operator<<(std::ostream &out, const NetID &id)
  {
    return id.print(out, -1, -1);
  }

  /// RouterContact
  struct RouterContact final : public IBEncodeMessage, public util::IStateful
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

    struct Hash
    {
      size_t
      operator()(const RouterContact &r) const
      {
        return PubKey::Hash()(r.pubkey);
      }
    };

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

    uint64_t last_updated = 0;

    util::StatusObject
    ExtractStatus() const override;

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
    DecodeKey(const llarp_buffer_t &k, llarp_buffer_t *buf) override;

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

    std::ostream &
    print(std::ostream &stream, int level, int spaces) const;

    bool
    Read(const char *fname);

    bool
    Write(const char *fname) const;

   private:
    bool
    VerifySignature(llarp::Crypto *crypto) const;
  };

  inline std::ostream &
  operator<<(std::ostream &out, const RouterContact &rc)
  {
    return rc.print(out, -1, -1);
  }

  using RouterLookupHandler =
      std::function< void(const std::vector< RouterContact > &) >;
}  // namespace llarp

#endif

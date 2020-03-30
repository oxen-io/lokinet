#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP

#include <constants/version.hpp>
#include <crypto/types.hpp>
#include <net/address_info.hpp>
#include <net/exit_info.hpp>
#include <util/aligned.hpp>
#include <util/bencode.hpp>
#include <util/status.hpp>
#include <router_version.hpp>

#include <functional>
#include <nlohmann/json.hpp>
#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace llarp
{
  /**
     NetID is an 8 byte field for isolating distinct networks
  */
  struct NetID final : public AlignedBuffer< 8 >
  {
    /**
       @brief the current default network id
     */
    static NetID &
    DefaultValue();

    /**
       @brief construct using default netid value
     */
    NetID();

    /**
       @brief construct from raw bytes
     */
    explicit NetID(const byte_t *val);

    /**
       @brief copy constructor
     */
    explicit NetID(const NetID &other) = default;

    /**
       @brief equality operator overload
     */
    bool
    operator==(const NetID &other) const;

    /**
       @brief non equality operator overload
     */
    bool
    operator!=(const NetID &other) const
    {
      return !(*this == other);
    }

    /**
       @brief used with operator overload on std::ostream
     */
    std::ostream &
    print(std::ostream &stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue(ToString());
      return stream;
    }

    /**
       @brief construct a string
     */
    std::string
    ToString() const;

    /**
       @brief de-serialize from buffer
    */
    bool
    BDecode(llarp_buffer_t *buf);

    /**
       @brief serialize to buffer
     */
    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  /** @brief operator overload for std::ostream printing of netid */
  inline std::ostream &
  operator<<(std::ostream &out, const NetID &id)
  {
    return id.print(out, -1, -1);
  }

  /**
     @brief Self Signed Router Descriptor
   */
  struct RouterContact
  {
    /// for unit tests
    static bool BlockBogons;

    /** default lifetime */
    static llarp_time_t Lifetime;
    /** default interval to update RCs at */
    static llarp_time_t UpdateInterval;
    /** age at which RC is considered stale */
    static llarp_time_t StaleInsertionAge;

    /**
      @brief default constructor
    */
    RouterContact()
    {
      Clear();
    }

    /**
       Hash specialization for router contact by its long term identity key
     */
    struct Hash
    {
      size_t
      operator()(const RouterContact &r) const
      {
        return PubKey::Hash()(r.pubkey);
      }
    };

    /** advertised addresses */
    std::vector< AddressInfo > addrs;
    /** network identifier */
    NetID netID;
    /** public encryption public key */
    llarp::PubKey enckey;
    /** public signing public key */
    llarp::PubKey pubkey;
    /** advertised exits */
    std::vector< ExitInfo > exits;
    /** signature */
    llarp::Signature signature;
    /** unused node nickname, yw kee */
    llarp::AlignedBuffer< NICKLEN > nickname;
    /** timestamp last signed at */
    llarp_time_t last_updated = 0s;
    /** protocol version */
    uint64_t version = LLARP_PROTO_VERSION;
    /** router's router version */
    nonstd::optional< RouterVersion > routerVersion;

    /**
       @brief introspect for RPC
     */
    util::StatusObject
    ExtractStatus() const;

    /**
       @brief convert to json object
     */
    nlohmann::json
    ToJson() const
    {
      return ExtractStatus();
    }

    /**
       @brief serialize to buffer
    */
    bool
    BEncode(llarp_buffer_t *buf) const;

    /** @brief equality operator overload */
    bool
    operator==(const RouterContact &other) const
    {
      return addrs == other.addrs && enckey == other.enckey
          && pubkey == other.pubkey && signature == other.signature
          && nickname == other.nickname && last_updated == other.last_updated
          && netID == other.netID;
    }

    /** @brief less than operator overload for std::set */
    bool
    operator<(const RouterContact &other) const
    {
      return pubkey < other.pubkey;
    }

    /** @brief inequality operator overload */
    bool
    operator!=(const RouterContact &other) const
    {
      return !(*this == other);
    }

    /** @brief clear internal members and zero everything out */
    void
    Clear();

    /** @brief return true if this router advertizes exits */
    bool
    IsExit() const
    {
      return !exits.empty();
    }

    /**
       @brief de-serialize from buffer
    */
    bool
    BDecode(llarp_buffer_t *buf)
    {
      Clear();
      return bencode_decode_dict(*this, buf);
    }

    /** @brief read key/value when deserializing dict */
    bool
    DecodeKey(const llarp_buffer_t &k, llarp_buffer_t *buf);

    /** @brief return true if nickname is set */
    bool
    HasNick() const;

    /** @brief get router's claimed nickname */
    std::string
    Nick() const;

    /** @brief return true if this descriptor seems to be for a public relay */
    bool
    IsPublicRouter() const;

    /** @brief set claimed nickname */
    void
    SetNick(string_view nick);

    /**
        @brief verify signature and validity of contents
        @param now the timestamp rights now
        @param allowExpired should we ignore time based expirations, defaults to
       true
    */
    bool
    Verify(llarp_time_t now, bool allowExpired = true) const;

    /**
        @brief signs contents with a secret key
        @returns false on serialize or sign failure, true on success
    */
    bool
    Sign(const llarp::SecretKey &secret);

    /// does this RC expire soon? default delta is 1 minute
    bool
    ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 1min) const;

    /// returns true if this RC is expired and should be removed
    bool
    IsExpired(llarp_time_t now) const;

    /// returns time in ms until we expire or 0 if we have expired
    llarp_time_t
    TimeUntilExpires(llarp_time_t now) const;

    /// get the age of this RC in ms
    llarp_time_t
    Age(llarp_time_t now) const;

    /**
       @brief determine if another RC is newer than our current one
     */
    bool
    OtherIsNewer(const RouterContact &other) const
    {
      return last_updated < other.last_updated;
    }

    /** @brief for std::ostream operator overload */
    std::ostream &
    print(std::ostream &stream, int level, int spaces) const;

    /** @brief read from file by name */
    bool
    Read(const char *fname);

    /** @brief write to file by name */
    bool
    Write(const char *fname) const;

    /** @brief verify signature only */
    bool
    VerifySignature() const;
  };

  inline std::ostream &
  operator<<(std::ostream &out, const RouterContact &rc)
  {
    return rc.print(out, -1, -1);
  }

  /**
      @brief hook function handling a router lookup that can return 0 or 1
      RouterContacts
  */
  using RouterLookupHandler =
      std::function< void(const std::vector< RouterContact > &) >;
}  // namespace llarp

#endif

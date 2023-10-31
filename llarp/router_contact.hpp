#pragma once

#include "router_id.hpp"
#include "router_version.hpp"

#include <llarp/constants/version.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/dns/srv_data.hpp>
#include <llarp/net/exit_info.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/status.hpp>

#include <nlohmann/json.hpp>
#include <oxenc/bt_producer.h>
#include <quic.hpp>

#include <functional>
#include <vector>

namespace oxenc
{
  class bt_list_consumer;
}  // namespace oxenc

/*
  - figure out where we do bt_encoding of RC's
    - maybe dump secret key from bt_encode
      - it's weird to pass the secret key contextually
    - suspicion we will need optional signature as an optional(?) string with serialized data
      - resetting signature would be string::clear() instead
      - ::sign() will cache serialized value
  - do timestamp stuff
  - bt_encode that takes bt_dict_producer requires reference to subdict
    - presumably to be done in endpoints
    - will be used for get_multi_rc endpoint
*/

namespace llarp
{
  static auto logcat = log::Cat("RC");

  using rc_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

  /// RouterContact
  struct RouterContact
  {
    static constexpr uint8_t RC_VERSION = 0;

    /// Constructs an empty RC
    RouterContact() = default;
    RouterContact(std::string)
    {
      log::error(logcat, "ERROR: SUPPLANT THIS CONSTRUCTOR");
    }

    // RouterContact(std::string buf);

    /// RC version that we support; we fail to load RCs that don't have the same version as that
    /// means they are incompatible with us.
    // static constexpr uint8_t VERSION = 0;

    /// Unit tests disable this to allow private IP ranges in RCs, which normally get rejected.
    static inline bool BLOCK_BOGONS = true;

    static inline std::string ACTIVE_NETID{LOKINET_DEFAULT_NETID};

    static inline constexpr size_t MAX_RC_SIZE = 1024;

    /// Timespans for RCs:

    /// How long (relative to its timestamp) before an RC becomes stale.  Stale records are used
    /// (e.g. for path building) only if there are no non-stale records available, such as might be
    /// the case when a client has been turned off for a while.
    static constexpr auto STALE = 12h;

    /// How long before an RC becomes invalid (and thus deleted).
    static constexpr auto LIFETIME = 30 * 24h;

    /// How long before a relay updates and re-publish its RC to the network.  (Relays can
    /// re-publish more frequently than this if needed; this is meant to apply only if there are no
    /// changes i.e. just to push out a new confirmation of the details).
    static constexpr auto REPUBLISH = STALE / 2 - 5min;

    /// Getters for private attributes
    const oxen::quic::Address&
    addr() const
    {
      return _addr;
    }

    const std::optional<oxen::quic::Address>&
    addr6() const
    {
      return _addr6;
    }

    const RouterID&
    router_id() const
    {
      return _router_id;
    }

    const rc_time&
    timestamp() const
    {
      return _timestamp;
    }

   protected:
    // advertised addresses
    oxen::quic::Address _addr;                  // refactor all 15 uses to use addr() method
    std::optional<oxen::quic::Address> _addr6;  // optional ipv6
    // public signing public key
    RouterID _router_id;  // refactor all 103 uses to use router_id() method

    rc_time _timestamp{};

    // Lokinet version at the time the RC was produced
    std::array<uint8_t, 3> _router_version;

   public:
    /// should we serialize the exit info?
    const static bool serializeExit = true;

    ustring _signed_payload;

    util::StatusObject
    extract_status() const;

    nlohmann::json
    to_json() const
    {
      return extract_status();
    }

    virtual std::string
    to_string() const
    {
      return fmt::format(
        "[RC k={} updated={} v={} addr={}]",
        _router_id,
        _timestamp,
        RC_VERSION,
        _addr.to_string());
    }

    /// On the wire we encode the data as a dict containing:
    /// ""  -- the RC format version, which must be == RouterContact::Version for us to attempt to
    ///        parse the reset of the fields.  (Future versions might have backwards-compat support
    ///        for lower versions).
    /// "4" -- 6 byte packed IPv4 address & port: 4 bytes of IPv4 address followed by 2 bytes of
    ///        port, both encoded in network (i.e. big-endian) order.
    /// "6" -- optional 18 byte IPv6 address & port: 16 byte raw IPv6 address followed by 2 bytes
    ///        of port in network order.
    /// "i" -- optional network ID string of up to 8 bytes; this is omitted for the default network
    ///        ID ("lokinet") but included for others (such as "gamma" for testnet).
    /// "p" -- 32-byte router pubkey
    /// "t" -- timestamp when this RC record was created (which also implicitly determines when it
    ///        goes stale and when it expires).
    /// "v" -- lokinet version of the router; this is a three-byte packed value of
    ///        MAJOR, MINOR, PATCH, e.g. \x00\x0a\x03 for 0.10.3.
    /// "~" -- signature of all of the previous serialized data, signed by "p"
    std::string
    bt_encode() const;

    virtual void
    bt_encode(oxenc::bt_dict_producer& btdp) const
    {
      (void)btdp;
    }

    bool
    operator==(const RouterContact& other) const
    {
      return _router_id == other._router_id and _addr == other._addr and _addr6 == other._addr6
          and _timestamp == other._timestamp and _router_version == other._router_version;
    }

    bool
    operator<(const RouterContact& other) const
    {
      return _router_id < other._router_id;
    }

    bool
    operator!=(const RouterContact& other) const
    {
      return !(*this == other);
    }

    virtual void
    clear() 
    {}

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf);

    bool
    is_public_router() const;

    /// does this RC expire soon? default delta is 1 minute
    bool
    expires_within_delta(llarp_time_t now, llarp_time_t dlt = 1min) const;

    /// returns true if this RC is expired and should be removed
    bool
    is_expired(llarp_time_t now) const;

    /// returns time in ms until we expire or 0 if we have expired
    llarp_time_t
    time_to_expiry(llarp_time_t now) const;

    /// get the age of this RC in ms
    llarp_time_t
    age(llarp_time_t now) const;

    bool
    other_is_newer(const RouterContact& other) const
    {
      return _timestamp < other._timestamp;
    }

    bool
    read(const fs::path& fname);

    bool
    write(const fs::path& fname) const;

    bool
    is_obsolete_bootstrap() const;

    void
    bt_load(oxenc::bt_dict_consumer& data);

  };


  /// Extension of RouterContact used to store a local "RC," and inserts a RouterContact by
  /// re-parsing and sending it out. This sub-class contains a pubkey and all the other attributes
  /// required for signing and serialization
  ///
  /// Note: this class may be entirely superfluous, so it is used here as a placeholder until its
  /// marginal utility is determined. It may end up as a free-floating method that reads in
  /// parameters and outputs a bt-serialized string
  struct LocalRC : public RouterContact
  {
   private:
    ustring _signature;

    const SecretKey _secret_key;

    void
    bt_sign(oxenc::bt_dict_consumer& btdc);
    
    void
    resign();

   public:
    LocalRC(std::string payload, const SecretKey sk);

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    void
    clear() override
    {
      _addr = {};
      _addr6.reset();
      _router_id.Zero();
      _timestamp = {};
      _router_version.fill(0);
      _signature.clear();
    }

    bool
    operator==(const LocalRC& other) const
    {
      return _router_id == other._router_id and _addr == other._addr and _addr6 == other._addr6
          and _timestamp == other._timestamp and _router_version == other._router_version
          and _signature == other._signature;
    }

    /// Mutators for the private member attributes. Calling on the mutators
    /// will clear the current signature and re-sign the RC
    void
    set_addr(oxen::quic::Address new_addr)
    {
      resign();
      _addr = std::move(new_addr);
    }

    void
    set_addr6(oxen::quic::Address new_addr)
    {
      resign();
      _addr6 = std::move(new_addr);
    }

    void
    set_router_id(RouterID rid)
    {
      resign();
      _router_id = std::move(rid);
    }

    void
    set_timestamp(llarp_time_t ts)
    {
      set_timestamp(rc_time{std::chrono::duration_cast<std::chrono::seconds>(ts)});
    }

    void
    set_timestamp(rc_time ts)
    {
      _timestamp = ts;
    }

    /// Sets RC timestamp to current system clock time
    void
    set_systime_timestamp()
    {
      set_timestamp(
          std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
    }
  };


  /// Extension of RouterContact used in a "read-only" fashion. Parses the incoming RC to query
  /// the data in the constructor, eliminating the need for a ::verify method/
  struct RemoteRC : public RouterContact
  {
   private:
    //

    void
    bt_verify(oxenc::bt_dict_consumer& data, bool reject_expired = false);

   public: 
    RemoteRC(std::string payload);

    // TODO: this method could use oxenc's append_encoded
    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;
    
    void
    clear() override
    {
      _addr = {};
      _addr6.reset();
      _router_id.Zero();
      _timestamp = {};
      _router_version.fill(0);
    }

  };

  template <>
  constexpr inline bool IsToStringFormattable<RouterContact> = true;
  template <>
  constexpr inline bool IsToStringFormattable<RemoteRC> = true;
  template <>
  constexpr inline bool IsToStringFormattable<LocalRC> = true;

  using RouterLookupHandler = std::function<void(const std::vector<RouterContact>&)>;
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::RouterContact>
  {
    size_t
    operator()(const llarp::RouterContact& r) const
    {
      return std::hash<llarp::PubKey>{}(r.router_id());
    }
  };
}  // namespace std

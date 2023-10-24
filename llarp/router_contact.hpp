#pragma once

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

#define MAX_RC_SIZE (1024)

namespace oxenc
{
  class bt_list_consumer;
}  // namespace oxenc

namespace llarp
{
  /// NetID
  struct NetID final : public AlignedBuffer<8>
  {
    static NetID&
    DefaultValue();

    NetID();

    explicit NetID(const byte_t* val);

    NetID(const NetID& other) = default;
    NetID&
    operator=(const NetID& other) = default;

    bool
    operator==(const NetID& other) const;

    bool
    operator!=(const NetID& other) const
    {
      return !(*this == other);
    }

    std::string
    ToString() const;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;
  };

  /// RouterContact
  struct RouterContact
  {
    /// for unit tests
    static bool BlockBogons;

    static llarp_time_t Lifetime;
    static llarp_time_t UpdateInterval;
    static llarp_time_t StaleInsertionAge;

    RouterContact()
    {
      Clear();
    }

    RouterContact(std::string buf);

    // advertised addresses
    oxen::quic::Address addr;
    // network identifier
    NetID netID;
    // public encryption public key
    llarp::PubKey enckey;
    // public signing public key
    llarp::PubKey pubkey;
    // signature
    llarp::Signature signature;

    llarp_time_t last_updated = 0s;
    uint64_t version = llarp::constants::proto_version;
    std::optional<RouterVersion> routerVersion;
    /// should we serialize the exit info?
    const static bool serializeExit = true;

    std::string signed_bt_dict;

    std::vector<dns::SRVData> srvRecords;

    util::StatusObject
    ExtractStatus() const;

    RouterID
    router_id() const
    {
      return pubkey;
    }

    nlohmann::json
    ToJson() const
    {
      return ExtractStatus();
    }

    std::string
    ToString() const;

    std::string
    bt_encode() const;

    void
    bt_encode_subdict(oxenc::bt_list_producer& btlp) const;

    std::string
    bencode_signed_section() const;

    std::string
    ToTXTRecord() const;

    bool
    operator==(const RouterContact& other) const
    {
      return addr == other.addr && enckey == other.enckey && pubkey == other.pubkey
          && signature == other.signature && last_updated == other.last_updated
          && netID == other.netID;
    }

    bool
    operator<(const RouterContact& other) const
    {
      return pubkey < other.pubkey;
    }

    bool
    operator!=(const RouterContact& other) const
    {
      return !(*this == other);
    }

    void
    Clear();

    bool
    IsExit() const
    {
      return false;
    }

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf);

    bool
    IsPublicRouter() const;

    bool
    Verify(llarp_time_t now, bool allowExpired = true) const;

    bool
    Sign(const llarp::SecretKey& secret);

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

    bool
    OtherIsNewer(const RouterContact& other) const
    {
      return last_updated < other.last_updated;
    }

    bool
    Read(const fs::path& fname);

    bool
    Write(const fs::path& fname) const;

    bool
    VerifySignature() const;

    /// return true if the netid in this rc is for the network id we are using
    bool
    FromOurNetwork() const;

    bool
    IsObsoleteBootstrap() const;

   private:
    bool
    DecodeVersion_0(llarp_buffer_t* buf);

    bool
    DecodeVersion_1(oxenc::bt_list_consumer& btlist);
  };

  template <>
  constexpr inline bool IsToStringFormattable<NetID> = true;
  template <>
  constexpr inline bool IsToStringFormattable<RouterContact> = true;

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
      return std::hash<llarp::PubKey>{}(r.pubkey);
    }
  };
}  // namespace std

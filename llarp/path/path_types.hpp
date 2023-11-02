#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/crypto/constants.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/aligned.hpp>

namespace
{
  static auto path_cat = llarp::log::Cat("lokinet.path");
}  // namespace

namespace llarp
{
  struct PathID_t final : public AlignedBuffer<PATHIDSIZE>
  {
    using AlignedBuffer<PATHIDSIZE>::AlignedBuffer;
  };

  namespace path
  {
    /// configuration for a single hop when building a path
    struct PathHopConfig
    {
      /// path id
      PathID_t txID, rxID;
      // router contact of router
      RemoteRC rc;
      // temp public encryption key
      SecretKey commkey;
      /// shared secret at this hop
      SharedSecret shared;
      /// hash of shared secret used for nonce mutation
      ShortHash nonceXOR;
      /// next hop's router id
      RouterID upstream;
      /// nonce for key exchange
      TunnelNonce nonce;
      // lifetime
      llarp_time_t lifetime = DEFAULT_LIFETIME;

      util::StatusObject
      ExtractStatus() const;
    };

    inline bool
    operator<(const PathHopConfig& lhs, const PathHopConfig& rhs)
    {
      return std::tie(lhs.txID, lhs.rxID, lhs.rc, lhs.upstream, lhs.lifetime)
          < std::tie(rhs.txID, rhs.rxID, rhs.rc, rhs.upstream, rhs.lifetime);
    }

    // milliseconds waiting between builds on a path per router
    static constexpr auto MIN_PATH_BUILD_INTERVAL = 500ms;
    static constexpr auto PATH_BUILD_RATE = 100ms;
  }  // namespace path
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::PathID_t> : hash<llarp::AlignedBuffer<llarp::PathID_t::SIZE>>
  {};
}  // namespace std

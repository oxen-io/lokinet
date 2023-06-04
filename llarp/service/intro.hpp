#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/status.hpp>

#include <iostream>

namespace llarp
{
  namespace service
  {
    struct Introduction
    {
      RouterID router;
      PathID_t pathID;
      llarp_time_t latency = 0s;
      llarp_time_t expiresAt = 0s;
      uint64_t version = llarp::constants::proto_version;

      util::StatusObject
      ExtractStatus() const;

      bool
      IsExpired(llarp_time_t now) const
      {
        return now >= expiresAt;
      }

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 30s) const
      {
        return IsExpired(now + dlt);
      }

      std::string
      ToString() const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        return bencode_decode_dict(*this, buf);
      }

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

      void
      Clear();

      bool
      operator<(const Introduction& other) const
      {
        return std::tie(expiresAt, pathID, router, version, latency)
            < std::tie(other.expiresAt, other.pathID, other.router, other.version, other.latency);
      }

      bool
      operator==(const Introduction& other) const
      {
        return pathID == other.pathID && router == other.router;
      }

      bool
      operator!=(const Introduction& other) const
      {
        return pathID != other.pathID || router != other.router;
      }
    };

    /// comparator for introset timestamp
    struct CompareIntroTimestamp
    {
      bool
      operator()(const Introduction& left, const Introduction& right) const
      {
        return left.expiresAt > right.expiresAt;
      }
    };
  }  // namespace service
}  // namespace llarp

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::service::Introduction> = true;

namespace std
{
  template <>
  struct hash<llarp::service::Introduction>
  {
    size_t
    operator()(const llarp::service::Introduction& i) const
    {
      return std::hash<llarp::PubKey>{}(i.router) ^ std::hash<llarp::PathID_t>{}(i.pathID);
    }
  };
}  // namespace std

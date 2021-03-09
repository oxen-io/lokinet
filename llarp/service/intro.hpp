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
      PubKey router;
      PathID_t pathID;
      llarp_time_t latency = 0s;
      llarp_time_t expiresAt = 0s;
      uint64_t version = LLARP_PROTO_VERSION;

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

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

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
        return expiresAt < other.expiresAt || pathID < other.pathID || router < other.router
            || version < other.version || latency < other.latency;
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

      struct Hash
      {
        size_t
        operator()(const Introduction& i) const
        {
          return PubKey::Hash()(i.router) ^ PathID_t::Hash()(i.pathID);
        }
      };
    };

    inline std::ostream&
    operator<<(std::ostream& out, const Introduction& i)
    {
      return i.print(out, -1, -1);
    }
  }  // namespace service
}  // namespace llarp

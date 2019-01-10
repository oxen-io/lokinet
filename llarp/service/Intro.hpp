#ifndef LLARP_SERVICE_INTRO_HPP
#define LLARP_SERVICE_INTRO_HPP

#include <crypto.hpp>
#include <path_types.hpp>
#include <util/bencode.hpp>

#include <iostream>

namespace llarp
{
  namespace service
  {
    struct Introduction final : public llarp::IBEncodeMessage
    {
      llarp::PubKey router;
      llarp::PathID_t pathID;
      uint64_t latency   = 0;
      uint64_t version   = 0;
      uint64_t expiresAt = 0;

      Introduction() = default;
      Introduction(const Introduction& other) : IBEncodeMessage(other.version)
      {
        router    = other.router;
        pathID    = other.pathID;
        latency   = other.latency;
        version   = other.version;
        expiresAt = other.expiresAt;
      }

      bool
      IsExpired(llarp_time_t now) const
      {
        return now >= expiresAt;
      }

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 15000) const
      {
        if(dlt)
          return now >= (expiresAt - dlt);
        return IsExpired(now);
      }

      ~Introduction();

      friend std::ostream&
      operator<<(std::ostream& out, const Introduction& i)
      {
        return out << "k=" << i.router << " p=" << i.pathID
                   << " v=" << i.version << " x=" << i.expiresAt;
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      void
      Clear();

      bool
      operator<(const Introduction& other) const
      {
        return expiresAt < other.expiresAt || pathID < other.pathID
            || router < other.router || version < other.version
            || latency < other.latency;
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
  }  // namespace service
}  // namespace llarp

#endif

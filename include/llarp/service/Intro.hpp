#ifndef LLARP_SERVICE_INTRO_HPP
#define LLARP_SERVICE_INTRO_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/path_types.hpp>

#include <iostream>

namespace llarp
{
  namespace service
  {
    struct Introduction : public llarp::IBEncodeMessage
    {
      llarp::PubKey router;
      llarp::PathID_t pathID;
      uint64_t latency   = 0;
      uint64_t version   = 0;
      uint64_t expiresAt = 0;

      Introduction() = default;
      Introduction(const Introduction& other)
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
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

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
        return !(*this == other);
      }

      struct Hash
      {
        size_t
        operator()(const Introduction& i) const
        {
          return *i.router.data_l() ^ *i.pathID.data_l();
        }
      };
    };
  }  // namespace service
}  // namespace llarp

#endif

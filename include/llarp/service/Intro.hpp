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

      bool
      operator<(const Introduction& other) const
      {
        return expiresAt < other.expiresAt || pathID < other.pathID;
      }
    };
  }  // namespace service
}  // namespace llarp

#endif
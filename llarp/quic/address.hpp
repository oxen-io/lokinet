#pragma once

#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <iosfwd>

#include <ngtcp2/ngtcp2.h>

#include <llarp/net/sock_addr.hpp>
#include <llarp/service/convotag.hpp>

namespace llarp::quic
{
  union sockaddr_any
  {
    sockaddr_storage storage;
    sockaddr sa;
    sockaddr_in6 in6;
    sockaddr_in in;
  };

  class Address
  {
    sockaddr_in6 saddr{};
    ngtcp2_addr a{0, reinterpret_cast<sockaddr*>(&saddr), nullptr};

   public:
    Address() = default;
    Address(service::ConvoTag tag);

    Address(const Address& other)
    {
      *this = other;
    }

    service::ConvoTag
    Tag() const;

    Address&
    operator=(const Address&);

    operator sockaddr*()
    {
      return reinterpret_cast<sockaddr*>(&saddr);
    }

    operator const sockaddr*() const
    {
      return reinterpret_cast<const sockaddr*>(&saddr);
    }

    operator ngtcp2_addr&()
    {
      return a;
    }

    operator const ngtcp2_addr&() const
    {
      return a;
    }

    size_t
    sockaddr_size() const
    {
      return sizeof(sockaddr_in6);
    }

    std::string
    to_string() const;
  };

  // Wraps an ngtcp2_path (which is basically just and address pair) with remote/local components.
  // Implicitly convertable to a ngtcp2_path* so that this can be passed wherever a ngtcp2_path* is
  // taken in the ngtcp2 API.
  struct Path
  {
   private:
    Address local_, remote_;

   public:
    ngtcp2_path path{
        {local_.sockaddr_size(), local_, nullptr}, {remote_.sockaddr_size(), remote_, nullptr}};

    // Public accessors are const:
    const Address& local = local_;
    const Address& remote = remote_;

    Path() = default;
    Path(const Address& laddr, const Address& raddr) : local_{laddr}, remote_{raddr}
    {}

    Path(const Path& p) : Path{p.local, p.remote}
    {}

    Path&
    operator=(const Path& p)
    {
      local_ = p.local_;
      remote_ = p.remote_;
      return *this;
    }

    // Equivalent to `&obj.path`, but slightly more convenient for passing into ngtcp2 functions
    // taking a ngtcp2_path pointer.
    operator ngtcp2_path*()
    {
      return &path;
    }
    operator const ngtcp2_path*() const
    {
      return &path;
    }

    std::string
    to_string() const;
  };

  std::ostream&
  operator<<(std::ostream& o, const Address& a);

  std::ostream&
  operator<<(std::ostream& o, const Path& p);

}  // namespace llarp::quic

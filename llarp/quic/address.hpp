#pragma once

#include <llarp/net/sock_addr.hpp>

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
  // Wrapper around a sockaddr; ngtcp2 requires more intrusive access that llarp::SockAddr is meant
  // to deal with, hence this wrapper (rather than trying to abuse llarp::SockAddr).
  class Address
  {
    sockaddr_in6 saddr{};
    ngtcp2_addr a{sizeof(saddr), reinterpret_cast<sockaddr*>(&saddr)};

   public:
    Address() = default;
    Address(const SockAddr& addr);

    Address(const Address& other)
    {
      *this = other;
    }

    Address&
    operator=(const Address&);

    // Implicit conversion to sockaddr* and ngtcp2_addr& so that an Address can be passed wherever
    // one of those is expected.
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

    // Implicit conversion to a convo tag so you can pass an Address to things taking a ConvoTag
    operator service::ConvoTag() const;

    // Returns the lokinet pseudo-port for the quic connection (which routes this quic packet to the
    // correct waiting quic instance on the remote).
    nuint16_t
    port() const
    {
      return nuint16_t{saddr.sin6_port};
    }

    // Sets the address port
    void
    port(nuint16_t port)
    {
      saddr.sin6_port = port.n;
    }

    // Implicit conversion to SockAddr for going back to general llarp code
    // FIXME: see if this is still needed, I think it may have been refactored away with the
    // ConvoTag operator
    operator SockAddr() const
    {
      return SockAddr(saddr);
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
    ngtcp2_path path{{local_.sockaddr_size(), local_}, {remote_.sockaddr_size(), remote_}, nullptr};

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

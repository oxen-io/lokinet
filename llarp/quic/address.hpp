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
    ngtcp2_addr a{reinterpret_cast<sockaddr*>(&saddr), sizeof(saddr)};

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
    // one of those is expected.  Templatized so that implicit conversion to other things doesn't
    // happen.
    template <typename T, std::enable_if_t<std::is_same_v<T, sockaddr>, int> = 0>
    operator T*()
    {
      return reinterpret_cast<sockaddr*>(&saddr);
    }

    template <typename T, std::enable_if_t<std::is_same_v<T, sockaddr>, int> = 0>
    operator const T*() const
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
    ToString() const;
  };

  // Wraps an ngtcp2_path (which is basically just and address pair) with remote/local components.
  // Implicitly convertable to a ngtcp2_path* so that this can be passed wherever a ngtcp2_path* is
  // taken in the ngtcp2 API.
  struct Path
  {
   private:
    Address local_, remote_;

   public:
    ngtcp2_path path{{local_, local_.sockaddr_size()}, {remote_, remote_.sockaddr_size()}, nullptr};

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
    // taking a ngtcp2_path pointer.  Templatized to prevent implicit conversion to other type of
    // pointers/ints.
    template <typename T, std::enable_if_t<std::is_same_v<T, ngtcp2_path>, int> = 0>
    operator T*()
    {
      return &path;
    }
    template <typename T, std::enable_if_t<std::is_same_v<T, ngtcp2_path>, int> = 0>
    operator const T*() const
    {
      return &path;
    }

    std::string
    ToString() const;
  };
}  // namespace llarp::quic

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::quic::Address> = true;
template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::quic::Path> = true;

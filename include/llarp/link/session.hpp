#ifndef LLARP_LINK_SESSION_HPP
#define LLARP_LINK_SESSION_HPP

#include <llarp/crypto.hpp>
#include <llarp/net.hpp>

namespace llarp
{
  struct LinkIntroMessage;
  struct ILinkMessage;

  struct ILinkSession
  {
    virtual ~ILinkSession(){};

    /// called every event loop tick
    virtual void
    Pump() = 0;

    /// called every timer tick
    virtual void
    Tick(llarp_time_t now) = 0;

    /// handle a link intro message
    virtual bool
    HandleLinkIntro(const LinkIntroMessage* msg) = 0;

    /// send a message buffer to the remote endpoint
    virtual bool
    SendMessageBuffer(llarp_buffer_t buf) = 0;

    /// handle low level recv of data
    virtual bool
    Recv(const void* buf, size_t sz) = 0;

    /// send handshake
    virtual void
    Handshake() = 0;

    /// send a keepalive to the remote endpoint
    virtual bool
    SendKeepAlive() = 0;

    /// send close message
    virtual void
    SendClose() = 0;

    /// return true if we are established
    virtual bool
    IsEstablished() const = 0;

    /// return true if this session has timed out
    virtual bool
    TimedOut(llarp_time_t now) const = 0;

    /// get remote public identity key
    virtual const PubKey&
    GetPubKey() const = 0;

    /// get remote address endpoint
    virtual const Addr&
    GetRemoteEndpoint() const = 0;
  };
}  // namespace llarp

#endif

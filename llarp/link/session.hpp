#ifndef LLARP_LINK_SESSION_HPP
#define LLARP_LINK_SESSION_HPP

#include <crypto.hpp>
#include <net.hpp>
#include <router_contact.hpp>
#include <types.hpp>

#include <functional>

namespace llarp
{
  struct LinkIntroMessage;
  struct ILinkMessage;
  struct ILinkLayer;
  struct ILinkSession
  {
    virtual ~ILinkSession(){};

    /// called every event loop tick
    std::function< void(void) > Pump;

    /// called every timer tick
    std::function< void(llarp_time_t) > Tick;

    /// send a message buffer to the remote endpoint
    std::function< bool(llarp_buffer_t) > SendMessageBuffer;

    /// start the connection
    std::function< void(void) > Start;

    /// send a keepalive to the remote endpoint
    std::function< bool(void) > SendKeepAlive;

    /// send close message
    std::function< void(void) > SendClose;

    /// return true if we are established
    std::function< bool(void) > IsEstablished;

    /// return true if this session has timed out
    std::function< bool(llarp_time_t) > TimedOut;

    /// get remote public identity key
    std::function< const byte_t *(void) > GetPubKey;

    /// get remote address
    std::function< const Addr &(void) > GetRemoteEndpoint;

    // get remote rc
    std::function< llarp::RouterContact(void) > GetRemoteRC;

    /// handle a valid LIM
    std::function< bool(const LinkIntroMessage *msg) > GotLIM;

    /// send queue current blacklog
    std::function< size_t(void) > SendQueueBacklog;

    /// get parent link layer
    std::function< ILinkLayer *(void) > GetLinkLayer;
  };
}  // namespace llarp

#endif

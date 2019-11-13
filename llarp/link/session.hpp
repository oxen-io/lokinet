#ifndef LLARP_LINK_SESSION_HPP
#define LLARP_LINK_SESSION_HPP

#include <crypto/types.hpp>
#include <net/net.hpp>
#include <ev/ev.hpp>
#include <router_contact.hpp>
#include <util/types.hpp>

#include <functional>

namespace llarp
{
  struct LinkIntroMessage;
  struct ILinkMessage;
  struct ILinkLayer;

  struct PacketEvent
  {
    static constexpr size_t MaxMTU = 1500;
    std::array< byte_t, MaxMTU > _data;
    size_t _sz = 0;
    size_t Index;

    void
    Load(uint64_t s, byte_t *ptr, size_t sz)
    {
      if(sz > MaxMTU)
      {
        assert(false);
        return;
      }
      seqno = s;
      _sz   = sz;
      if(ptr)
        std::copy_n(ptr, sz, _data.data());
      assert(_sz <= MaxMTU);
    }

    void
    Clear()
    {
      _sz   = 0;
      seqno = 0;
    }

    byte_t *
    Data()
    {
      return _data.data();
    }

    const byte_t *
    Data() const
    {
      return _data.data();
    }

    size_t
    Size() const
    {
      assert(_sz <= MaxMTU);
      return _sz;
    }

    uint64_t seqno;

    bool
    operator<(const PacketEvent &other) const
    {
      return other.seqno < seqno;
    }
    using Ptr_t = PacketEvent *;
  };

  struct ILinkSession
  {
    virtual ~ILinkSession() = default;

    /// delivery status of a message
    enum class DeliveryStatus
    {
      eDeliverySuccess = 0,
      eDeliveryDropped = 1
    };

    /// equiv of shared_from_this but for the interface type so
    /// that each implementation can use shared_from_this
    virtual std::shared_ptr< ILinkSession >
    BorrowSelf() = 0;

    /// hook for utp for when we have established a connection
    virtual void
    OnLinkEstablished(ILinkLayer *){};

    /// called every event loop tick
    virtual void
    Pump() = 0;

    /// called every timer tick
    virtual void Tick(llarp_time_t) = 0;

    /// message delivery result hook function
    using CompletionHandler = std::function< void(DeliveryStatus) >;

    using Packet_t  = PacketEvent::Ptr_t;
    using Message_t = std::vector< byte_t >;

    /// send a message buffer to the remote endpoint
    virtual bool
    SendMessageBuffer(Message_t, CompletionHandler handler) = 0;

    /// start the connection
    virtual void
    Start() = 0;

    virtual void
    Close() = 0;

    /// recv packet on low layer
    /// not used by utp
    virtual void
    Recv_LL(byte_t *, size_t)
    {
    }

    /// send a keepalive to the remote endpoint
    virtual bool
    SendKeepAlive() = 0;

    /// return true if we are established
    virtual bool
    IsEstablished() const = 0;

    /// return true if this session has timed out
    virtual bool
    TimedOut(llarp_time_t now) const = 0;

    /// get remote public identity key
    virtual PubKey
    GetPubKey() const = 0;

    /// get remote address
    virtual Addr
    GetRemoteEndpoint() const = 0;

    // get remote rc
    virtual RouterContact
    GetRemoteRC() const = 0;

    /// handle a valid LIM
    std::function< bool(const LinkIntroMessage *msg) > GotLIM;

    /// send queue current blacklog
    virtual size_t
    SendQueueBacklog() const = 0;

    /// get parent link layer
    virtual ILinkLayer *
    GetLinkLayer() const = 0;

    /// renegotiate session when we have a new RC locally
    virtual bool
    RenegotiateSession() = 0;

    /// return true if we should send an explicit keepalive message
    virtual bool
    ShouldPing() const = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };
}  // namespace llarp

#endif

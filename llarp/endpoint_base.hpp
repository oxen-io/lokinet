#pragma once

#include "llarp/service/address.hpp"
#include "llarp/service/convotag.hpp"
#include "llarp/service/protocol_type.hpp"
#include "router_id.hpp"
#include "ev/ev.hpp"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include "oxenmq/variant.h"

namespace llarp
{
  namespace quic
  {
    class TunnelManager;
  }

  class EndpointBase
  {
   public:
    virtual ~EndpointBase() = default;

    using AddressVariant_t = std::variant<service::Address, RouterID>;

    struct SendStat
    {
      /// how many routing messages we sent to them
      uint64_t messagesSend;
      /// how many routing messages we got from them
      uint64_t messagesRecv;
      /// how many convos have we had to this guy total?
      size_t numTotalConvos;
      /// current estimated rtt
      Duration_t estimatedRTT;
      /// last time point when we sent a message to them
      Duration_t lastSendAt;
      /// last time point when we got a message from them
      Duration_t lastRecvAt;
    };

    /// get statistics about how much traffic we sent and recv'd via remote endpoints we are talking
    /// to
    virtual std::unordered_map<AddressVariant_t, SendStat>
    GetStatistics() const = 0;

    /// get our local address
    virtual AddressVariant_t
    LocalAddress() const = 0;

    virtual quic::TunnelManager*
    GetQUICTunnel() = 0;

    virtual std::optional<AddressVariant_t>
    GetEndpointWithConvoTag(service::ConvoTag tag) const = 0;

    virtual std::optional<service::ConvoTag>
    GetBestConvoTagFor(AddressVariant_t addr) const = 0;

    virtual bool
    EnsurePathTo(
        AddressVariant_t addr,
        std::function<void(std::optional<service::ConvoTag>)> hook,
        llarp_time_t timeout) = 0;

    virtual void
    LookupNameAsync(
        std::string name, std::function<void(std::optional<AddressVariant_t>)> resultHandler) = 0;

    virtual const EventLoop_ptr&
    Loop() = 0;

    virtual bool
    SendToOrQueue(
        service::ConvoTag tag, const llarp_buffer_t& payload, service::ProtocolType t) = 0;
  };

}  // namespace llarp

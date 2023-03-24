#pragma once

#include "router_contact.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <set>

#include <oxenc/variant.h>
#include <llarp/config/config.hpp>
#include <llarp/service/address.hpp>
#include <llarp/service/convotag.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/ev/ev.hpp>
#include <vector>

namespace llarp
{
  namespace quic
  {
    class TunnelManager;
  }

  namespace dns
  {
    class Server;
  }

  class [[deprecated]] EndpointBase
  {
    std::unordered_set<dns::SRVData> m_SRVRecords;

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

    /// info about a quic mapping
    struct QUICMappingInfo
    {
      /// srv data if it was provided
      std::optional<dns::SRVData> srv;
      /// address we are bound on
      SockAddr localAddr;
      /// the remote's lns name if we have one
      std::optional<std::string> remoteName;
      /// the remote's address
      AddressVariant_t remoteAddr;
      /// the remote's port we are connecting to
      uint16_t remotePort;
    };

    /// maybe get quic mapping info given its stream id
    /// returns std::nullopt if we have no stream given that id
    std::optional<QUICMappingInfo>
    GetQUICMappingInfoByID(int stream_id) const;

    /// add an srv record to this endpoint's descriptor
    void
    PutSRVRecord(dns::SRVData srv);

    /// get dns serverr if we have on on this endpoint
    virtual std::shared_ptr<dns::Server>
    DNS() const
    {
      return nullptr;
    };

    /// called when srv data changes in some way
    virtual void
    SRVRecordsChanged() = 0;

    /// remove srv records from this endpoint that match a filter
    /// for each srv record call it with filter, remove if filter returns true
    /// return if we removed any srv records
    bool
    DelSRVRecordIf(std::function<bool(const dns::SRVData&)> filter);

    /// get copy of all srv records
    std::set<dns::SRVData>
    SRVRecords() const;

    /// get statistics about how much traffic we sent and recv'd to a remote endpoint
    virtual std::optional<SendStat>
    GetStatFor(AddressVariant_t remote) const = 0;

    /// list all remote endpoint addresses we have that are mapped
    virtual std::unordered_set<AddressVariant_t>
    AllRemoteEndpoints() const = 0;

    /// get our local address
    virtual AddressVariant_t
    LocalAddress() const = 0;

    virtual quic::TunnelManager*
    GetQUICTunnel() = 0;

    virtual std::optional<AddressVariant_t>
    GetEndpointWithConvoTag(service::ConvoTag tag) const = 0;

    virtual std::optional<service::ConvoTag>
    GetBestConvoTagFor(AddressVariant_t addr) const = 0;

    /// MUST call MarkAddressOutbound first if we are initiating the flow to the remote.
    // TODO: this requirement is stupid.
    virtual bool
    EnsurePathTo(
        AddressVariant_t addr,
        std::function<void(std::optional<service::ConvoTag>)> hook,
        llarp_time_t timeout) = 0;

    virtual void
    LookupNameAsync(
        std::string name, std::function<void(std::optional<AddressVariant_t>)> resultHandler) = 0;

    virtual bool
    LookupRC(RouterID rid, RouterLookupHandler handler) = 0;

    virtual const EventLoop_ptr&
    Loop() = 0;

    virtual bool
    SendToOrQueue(service::ConvoTag tag, std::vector<byte_t> data, service::ProtocolType t) = 0;

    bool
    SendToOrQueue(service::ConvoTag tag, const llarp_buffer_t& payload, service::ProtocolType t)
    {
      return SendToOrQueue(std::move(tag), payload.copy(), t);
    }

    /// lookup srv records async
    virtual void
    LookupServiceAsync(
        std::string name,
        std::string service,
        std::function<void(std::vector<dns::SRVData>)> resultHandler) = 0;

    /// will mark a remote as something we want to initiate a flow to.
    /// making this explicit prevents us from doing weird things when we are the recipient.
    virtual void
    MarkAddressOutbound(AddressVariant_t remote) = 0;

    virtual bool
    Configure(const NetworkConfig&, const DnsConfig&) = 0;

    virtual std::string_view
    endpoint_name() const = 0;

    /// called to do any deferred startup items after configure.
    virtual void
    start_endpoint(){};

    /// initiate shutdown of all periodic operations.
    virtual void
    stop_endpoint(){};

    /// called in a periodic event loop timer.
    virtual void
    periodic_tick(){};

    /// called in an idempotent event loop handler after reading io each tick.
    virtual void
    event_loop_pump(){};
  };

}  // namespace llarp

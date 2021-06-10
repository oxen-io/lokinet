#pragma once

#include "llarp/service/address.hpp"
#include "llarp/service/convotag.hpp"
#include "llarp/service/protocol_type.hpp"
#include "router_id.hpp"
#include "llarp/ev/ev.hpp"
#include "llarp/dns/srv_data.hpp"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <unordered_set>
#include <set>
#include "oxenmq/variant.h"

namespace llarp
{
  namespace quic
  {
    class TunnelManager;
  }

  class EndpointBase
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

    /// lookup srv records async
    virtual void
    LookupServiceAsync(
        std::string name,
        std::string service,
        std::function<void(std::vector<dns::SRVData>)> resultHandler) = 0;

    virtual void
    MarkAddressOutbound(AddressVariant_t remote) = 0;
  };

}  // namespace llarp

#ifndef LLARP_SERVICE_HANDLER_HPP
#define LLARP_SERVICE_HANDLER_HPP

#include <crypto/types.hpp>
#include <path/path.hpp>
#include <service/intro_set.hpp>
#include <util/aligned.hpp>

#include <memory>
#include <set>

namespace llarp
{
  namespace service
  {
    using ConvoTag = AlignedBuffer<16>;
    struct ProtocolMessage;

    struct RecvDataEvent
    {
      path::Path_ptr fromPath;
      PathID_t pathid;
      std::shared_ptr<ProtocolMessage> msg;
    };

    struct ProtocolMessage;
    struct IDataHandler
    {
      virtual bool
      HandleDataMessage(
          path::Path_ptr path, const PathID_t from, std::shared_ptr<ProtocolMessage> msg) = 0;

      virtual bool
      GetCachedSessionKeyFor(const ConvoTag& remote, SharedSecret& secret) const = 0;
      virtual void
      PutCachedSessionKeyFor(const ConvoTag& remote, const SharedSecret& secret) = 0;

      virtual void
      MarkConvoTagActive(const ConvoTag& tag) = 0;

      virtual void
      RemoveConvoTag(const ConvoTag& remote) = 0;

      virtual bool
      HasConvoTag(const ConvoTag& remote) const = 0;

      virtual void
      PutSenderFor(const ConvoTag& remote, const ServiceInfo& si, bool inbound) = 0;

      virtual bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const = 0;

      virtual void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro) = 0;

      virtual bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const = 0;

      virtual void
      PutReplyIntroFor(const ConvoTag& remote, const Introduction& intro) = 0;

      virtual bool
      GetReplyIntroFor(const ConvoTag& remote, Introduction& intro) const = 0;

      virtual bool
      GetConvoTagsForService(const Address& si, std::set<ConvoTag>& tag) const = 0;

      virtual bool
      HasInboundConvo(const Address& addr) const = 0;

      /// do we want a session outbound to addr
      virtual bool
      WantsOutboundSession(const Address& addr) const = 0;

      virtual void
      MarkAddressOutbound(const Address& addr) = 0;

      virtual void
      QueueRecvData(RecvDataEvent ev) = 0;
    };
  }  // namespace service
}  // namespace llarp

#endif

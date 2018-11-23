#ifndef LLARP_SERVICE_HANDLER_HPP
#define LLARP_SERVICE_HANDLER_HPP
#include <llarp/aligned.hpp>
#include <llarp/crypto.hpp>
#include <llarp/service/IntroSet.hpp>
#include <llarp/path_types.hpp>

namespace llarp
{
  namespace service
  {
    using ConvoTag = llarp::AlignedBuffer< 16 >;

    struct ProtocolMessage;
    struct IDataHandler
    {
      virtual bool
      HandleDataMessage(const PathID_t&, ProtocolMessage* msg) = 0;

      virtual bool
      GetCachedSessionKeyFor(const ConvoTag& remote,
                             const byte_t*& secret) const = 0;
      virtual void
      PutCachedSessionKeyFor(const ConvoTag& remote,
                             const SharedSecret& secret) = 0;

      virtual void
      PutSenderFor(const ConvoTag& remote, const ServiceInfo& si) = 0;

      virtual bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const = 0;

      virtual void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro) = 0;

      virtual bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const = 0;

      virtual bool
      GetConvoTagsForService(const ServiceInfo& si,
                             std::set< ConvoTag >& tag) const = 0;
    };
  }  // namespace service
}  // namespace llarp

#endif

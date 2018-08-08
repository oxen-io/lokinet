#ifndef LLARP_SERVICE_HANDLER_HPP
#define LLARP_SERVICE_HANDLER_HPP
#include <llarp/service/protocol.hpp>

namespace llarp
{
  namespace service
  {
    struct IDataHandler
    {
      virtual void
      HandleDataMessage(ProtocolMessage* msg) = 0;
    };
  }  // namespace service
}  // namespace llarp

#endif
#ifndef LLARP_SERVICE_SENDCONTEXT_HPP
#define LLARP_SERVICE_SENDCONTEXT_HPP

#include <path/pathset.hpp>
#include <routing/path_transfer_message.hpp>
#include <service/intro.hpp>
#include <service/protocol.hpp>
#include <util/buffer.hpp>
#include <util/types.hpp>
#include <util/thread/queue.hpp>

#include <deque>

namespace llarp
{
  namespace service
  {
    struct ServiceInfo;
    struct Endpoint;
    struct Introduction;

    struct SendContext
    {
      SendContext(ServiceInfo ident, const Introduction& intro,
                  path::PathSet* send, Endpoint* ep);

      void
      AsyncEncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t);

      /// queue send a fully encrypted hidden service frame
      /// via a path
      bool
      Send(std::shared_ptr< ProtocolFrame > f, path::Path_ptr path);

      /// flush upstream traffic when in router thread
      void
      FlushUpstream();

      SharedSecret sharedKey;
      ServiceInfo remoteIdent;
      Introduction remoteIntro;
      ConvoTag currentConvoTag;
      path::PathSet* const m_PathSet;
      IDataHandler* const m_DataHandler;
      Endpoint* const m_Endpoint;
      uint64_t sequenceNo       = 0;
      llarp_time_t lastGoodSend = 0;
      llarp_time_t createdAt;
      llarp_time_t sendTimeout    = 40 * 1000;
      llarp_time_t connectTimeout = 60 * 1000;
      bool markedBad              = false;
      using Msg_ptr     = std::shared_ptr< const routing::PathTransferMessage >;
      using SendEvent_t = std::pair< Msg_ptr, path::Path_ptr >;
      thread::Queue< SendEvent_t > m_SendQueue;

      virtual bool
      ShiftIntroduction(bool rebuild = true)
      {
        (void)rebuild;
        return true;
      }

      virtual void
      UpdateIntroSet(bool randomizePath = false) = 0;

      virtual bool
      MarkCurrentIntroBad(llarp_time_t now) = 0;

     private:
      void
      EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t);

      virtual void
      AsyncGenIntro(const llarp_buffer_t& payload, ProtocolType t) = 0;
    };
  }  // namespace service
}  // namespace llarp

#endif

#ifndef LLARP_SERVICE_SENDCONTEXT_HPP
#define LLARP_SERVICE_SENDCONTEXT_HPP

#include <path/pathset.hpp>
#include <service/Intro.hpp>
#include <service/protocol.hpp>
#include <util/buffer.hpp>
#include <util/types.hpp>

namespace llarp
{
  namespace service
  {
    struct ServiceInfo;
    struct Endpoint;
    struct Introduction;

    struct SendContext
    {
      SendContext(const ServiceInfo& ident, const Introduction& intro,
                  path::PathSet* send, Endpoint* ep);

      void
      AsyncEncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t);

      /// send a fully encrypted hidden service frame
      /// via a path on our pathset with path id p
      bool
      Send(const ProtocolFrame& f);

      SharedSecret sharedKey;
      ServiceInfo remoteIdent;
      Introduction remoteIntro;
      ConvoTag currentConvoTag;
      path::PathSet* m_PathSet;
      IDataHandler* m_DataHandler;
      Endpoint* m_Endpoint;
      uint64_t sequenceNo       = 0;
      llarp_time_t lastGoodSend = 0;
      llarp_time_t createdAt;
      llarp_time_t sendTimeout    = 40 * 1000;
      llarp_time_t connectTimeout = 60 * 1000;
      bool markedBad              = false;

      virtual bool
      ShiftIntroduction(bool rebuild = true)
      {
        (void)rebuild;
        return true;
      };

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

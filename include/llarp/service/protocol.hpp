#ifndef LLARP_SERVICE_PROTOCOL_HPP
#define LLARP_SERVICE_PROTOCOL_HPP
#include <llarp/time.h>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/encrypted.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/Identity.hpp>
#include <llarp/service/Info.hpp>
#include <llarp/service/Intro.hpp>
#include <llarp/service/handler.hpp>
#include <vector>

namespace llarp
{
  namespace service
  {
    constexpr std::size_t MAX_PROTOCOL_MESSAGE_SIZE = 2048;

    typedef uint64_t ProtocolType;

    constexpr ProtocolType eProtocolText    = 0UL;
    constexpr ProtocolType eProtocolTraffic = 1UL;

    /// inner message
    struct ProtocolMessage : public IBEncodeMessage
    {
      ProtocolMessage(const ConvoTag& tag);
      ProtocolMessage();
      ~ProtocolMessage();
      ProtocolType proto  = eProtocolText;
      llarp_time_t queued = 0;
      std::vector< byte_t > payload;
      Introduction introReply;
      ServiceInfo sender;
      IDataHandler* handler = nullptr;
      ConvoTag tag;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;

      void
      PutBuffer(llarp_buffer_t payload);

      static void
      ProcessAsync(void* user);
    };

    /// outer message
    struct ProtocolFrame : public llarp::routing::IMessage
    {
      llarp::PQCipherBlock C;
      llarp::Encrypted D;
      llarp::KeyExchangeNonce N;
      llarp::Signature Z;
      llarp::service::ConvoTag T;

      ProtocolFrame();
      ProtocolFrame(const ProtocolFrame& other);

      ~ProtocolFrame();

      bool
      EncryptAndSign(llarp_crypto* c, const ProtocolMessage& msg,
                     const byte_t* sharedkey, const Identity& localIdent);

      bool
      AsyncDecryptAndVerify(llarp_logic* logic, llarp_crypto* c,
                            llarp_threadpool* worker,
                            const Identity& localIdent,
                            IDataHandler* handler) const;

      bool
      DecryptPayloadInto(llarp_crypto* c, const byte_t* sharedkey,
                         ProtocolMessage& into) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      Verify(llarp_crypto* c, const ServiceInfo& from) const;

      bool
      HandleMessage(llarp::routing::IMessageHandler* h, llarp_router* r) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
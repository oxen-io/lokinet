#ifndef LLARP_SERVICE_PROTOCOL_HPP
#define LLARP_SERVICE_PROTOCOL_HPP
#include <llarp/time.hpp>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/encrypted.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/Identity.hpp>
#include <llarp/service/Info.hpp>
#include <llarp/service/Intro.hpp>
#include <llarp/service/handler.hpp>
#include <vector>

namespace llarp
{
  class Logic;

  namespace service
  {
    constexpr std::size_t MAX_PROTOCOL_MESSAGE_SIZE = 2048 * 2;

    using ProtocolType = uint64_t;

    constexpr ProtocolType eProtocolText    = 0UL;
    constexpr ProtocolType eProtocolTraffic = 1UL;

    /// inner message
    struct ProtocolMessage final : public IBEncodeMessage
    {
      ProtocolMessage(const ConvoTag& tag);
      ProtocolMessage();
      ~ProtocolMessage();
      ProtocolType proto  = eProtocolTraffic;
      llarp_time_t queued = 0;
      std::vector< byte_t > payload;
      Introduction introReply;
      ServiceInfo sender;
      IDataHandler* handler = nullptr;
      /// local path we got this message from
      PathID_t srcPath;
      ConvoTag tag;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      void
      PutBuffer(llarp_buffer_t payload);

      static void
      ProcessAsync(void* user);
    };

    /// outer message
    struct ProtocolFrame final : public llarp::routing::IMessage
    {
      llarp::PQCipherBlock C;
      llarp::Encrypted D;
      llarp::KeyExchangeNonce N;
      llarp::Signature Z;
      llarp::service::ConvoTag T;

      ProtocolFrame(const ProtocolFrame& other)
          : llarp::routing::IMessage()
          , C(other.C)
          , D(other.D)
          , N(other.N)
          , Z(other.Z)
          , T(other.T)
      {
        S       = other.S;
        version = other.version;
      }

      ProtocolFrame() : llarp::routing::IMessage()
      {
      }

      ~ProtocolFrame();

      bool
      operator==(const ProtocolFrame& other) const;

      bool
      operator!=(const ProtocolFrame& other) const
      {
        return !(*this == other);
      }

      ProtocolFrame&
      operator=(const ProtocolFrame& other);

      bool
      EncryptAndSign(llarp::Crypto* c, const ProtocolMessage& msg,
                     const byte_t* sharedkey, const Identity& localIdent);

      bool
      AsyncDecryptAndVerify(llarp::Logic* logic, llarp::Crypto* c,
                            const PathID_t& srcpath, llarp_threadpool* worker,
                            const Identity& localIdent,
                            IDataHandler* handler) const;

      bool
      DecryptPayloadInto(llarp::Crypto* c, const byte_t* sharedkey,
                         ProtocolMessage& into) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      Verify(llarp::Crypto* c, const ServiceInfo& from) const;

      bool
      HandleMessage(llarp::routing::IMessageHandler* h,
                    llarp::Router* r) const override;
    };
  }  // namespace service
}  // namespace llarp

#endif

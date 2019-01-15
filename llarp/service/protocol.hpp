#ifndef LLARP_SERVICE_PROTOCOL_HPP
#define LLARP_SERVICE_PROTOCOL_HPP

#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <dht/message.hpp>
#include <routing/message.hpp>
#include <service/Identity.hpp>
#include <service/Info.hpp>
#include <service/Intro.hpp>
#include <service/handler.hpp>
#include <util/bencode.hpp>
#include <util/time.hpp>

#include <vector>

struct llarp_threadpool;

namespace llarp
{
  struct Crypto;
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
    struct ProtocolFrame final : public routing::IMessage
    {
      using Encrypted_t = Encrypted< 2048 >;
      PQCipherBlock C;
      Encrypted_t D;
      KeyExchangeNonce N;
      Signature Z;
      service::ConvoTag T;

      ProtocolFrame(const ProtocolFrame& other)
          : routing::IMessage()
          , C(other.C)
          , D(other.D)
          , N(other.N)
          , Z(other.Z)
          , T(other.T)
      {
        S       = other.S;
        version = other.version;
      }

      ProtocolFrame() : routing::IMessage()
      {
        Clear();
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
      EncryptAndSign(Crypto* c, const ProtocolMessage& msg,
                     const SharedSecret& sharedkey, const Identity& localIdent);

      bool
      AsyncDecryptAndVerify(Logic* logic, Crypto* c, const PathID_t& srcpath,
                            llarp_threadpool* worker,
                            const Identity& localIdent,
                            IDataHandler* handler) const;

      bool
      DecryptPayloadInto(Crypto* c, const SharedSecret& sharedkey,
                         ProtocolMessage& into) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      void
      Clear() override
      {
        C.Zero();
        D.Clear();
        T.Zero();
        N.Zero();
        Z.Zero();
      }

      bool
      Verify(Crypto* c, const ServiceInfo& from) const;

      bool
      HandleMessage(routing::IMessageHandler* h, Router* r) const override;
    };
  }  // namespace service
}  // namespace llarp

#endif

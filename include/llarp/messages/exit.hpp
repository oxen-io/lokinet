#ifndef LLARP_MESSAGES_EXIT_HPP
#define LLARP_MESSAGES_EXIT_HPP
#include <llarp/routing/message.hpp>
#include <llarp/exit/policy.hpp>
#include <vector>
#include <llarp/crypto.hpp>

namespace llarp
{
  namespace routing
  {
    struct ObtainExitMessage final : public IMessage
    {
      std::vector< llarp::exit::Policy > B;
      uint64_t E;
      llarp::PubKey I;
      uint64_t T;
      std::vector< llarp::exit::Policy > W;
      llarp_time_t X;
      llarp::Signature Z;

      ObtainExitMessage() : IMessage()
      {
      }

      ~ObtainExitMessage()
      {
      }

      ObtainExitMessage&
      operator=(const ObtainExitMessage& other);

      /// populates I and signs
      bool
      Sign(llarp_crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp_crypto* c) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

    struct GrantExitMessage final : public IMessage
    {
      uint64_t T;
      GrantExitMessage() : IMessage()
      {
      }

      ~GrantExitMessage()
      {
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

    struct RejectExitMessage final : public IMessage
    {
      uint64_t B;
      std::vector< llarp::exit::Policy > R;
      uint64_t T;

      RejectExitMessage() : IMessage()
      {
      }

      ~RejectExitMessage()
      {
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

    struct UpdateExitVerifyMessage final : public IMessage
    {
      uint64_t T;

      UpdateExitVerifyMessage() : IMessage()
      {
      }

      ~UpdateExitVerifyMessage()
      {
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

    struct UpdateExitMessage final : public IMessage
    {
      llarp::PathID_t P;
      uint64_t T;
      llarp::Signature Z;

      UpdateExitMessage() : IMessage()
      {
      }

      ~UpdateExitMessage()
      {
      }

      UpdateExitMessage&
      operator=(const UpdateExitMessage& other);

      bool
      Sign(llarp_crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp_crypto* c, const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

    struct CloseExitMessage final : public IMessage
    {
      CloseExitMessage() : IMessage()
      {
      }

      ~CloseExitMessage()
      {
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };

  }  // namespace routing
}  // namespace llarp

#endif
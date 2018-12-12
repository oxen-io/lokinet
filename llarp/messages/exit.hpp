#ifndef LLARP_MESSAGES_EXIT_HPP
#define LLARP_MESSAGES_EXIT_HPP
#include <crypto.hpp>
#include <exit/policy.hpp>
#include <routing/message.hpp>

#include <vector>

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
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };

    struct GrantExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer< 16 >;
      uint64_t T;
      Nonce_t Y;
      llarp::Signature Z;
      GrantExitMessage() : IMessage()
      {
      }

      ~GrantExitMessage()
      {
      }

      GrantExitMessage&
      operator=(const GrantExitMessage& other);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c, const llarp::PubKey& pk) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };

    struct RejectExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer< 16 >;
      uint64_t B;
      std::vector< llarp::exit::Policy > R;
      uint64_t T;
      Nonce_t Y;
      llarp::Signature Z;

      RejectExitMessage() : IMessage()
      {
      }

      ~RejectExitMessage()
      {
      }

      RejectExitMessage&
      operator=(const RejectExitMessage& other);

      bool
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c, const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };

    struct UpdateExitVerifyMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer< 16 >;
      uint64_t T;
      Nonce_t Y;
      llarp::Signature Z;

      UpdateExitVerifyMessage() : IMessage()
      {
      }

      ~UpdateExitVerifyMessage()
      {
      }

      UpdateExitVerifyMessage&
      operator=(const UpdateExitVerifyMessage& other);

      bool
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c, const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };

    struct UpdateExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer< 16 >;
      llarp::PathID_t P;
      uint64_t T;
      Nonce_t Y;
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
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c, const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };

    struct CloseExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer< 16 >;

      Nonce_t Y;
      llarp::Signature Z;

      CloseExitMessage() : IMessage()
      {
      }

      ~CloseExitMessage()
      {
      }

      CloseExitMessage&
      operator=(const CloseExitMessage& other);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;

      bool
      Sign(llarp::Crypto* c, const llarp::SecretKey& sk);

      bool
      Verify(llarp::Crypto* c, const llarp::PubKey& pk) const;
    };

  }  // namespace routing
}  // namespace llarp

#endif

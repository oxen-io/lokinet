#pragma once

#include <cstdint>
#include <llarp/crypto/types.hpp>
#include "policy.hpp"
#include <llarp/routing/message.hpp>

#include <ratio>
#include <vector>

namespace llarp
{
  namespace routing
  {
    struct ObtainExitMessage final : public IMessage
    {
      std::vector<llarp::exit::Policy> blacklist;
      uint64_t wants_exit{0};
      llarp::PubKey source_identity;
      uint64_t txid{0};
      std::vector<llarp::exit::Policy> whitelist;
      uint64_t request_expires_at{0};
      llarp::Signature sig;

      decltype(blacklist)& B{blacklist};
      decltype(wants_exit)& E{wants_exit};
      decltype(source_identity)& I{source_identity};
      decltype(txid)& T{txid};
      decltype(whitelist)& W{whitelist};
      decltype(request_expires_at)& X{request_expires_at};
      decltype(sig)& Z{sig};

      ObtainExitMessage() : IMessage{}
      {}

      ~ObtainExitMessage() override = default;

      void
      Clear() override
      {
        IMessage::Clear();
        blacklist.clear();
        wants_exit = 0;
        source_identity.Zero();
        txid = 0;
        whitelist.clear();
        request_expires_at = 0;
        sig.Zero();
      }

      /// populates I and signs
      bool
      Sign(const llarp::SecretKey& sk);

      bool
      Verify() const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      std::string
      ToString() const;
    };

    struct GrantExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer<16>;

      uint64_t txid;
      Nonce_t nounce;
      llarp::Signature sig;

      decltype(txid)& T{txid};
      decltype(nounce)& Y{nounce};
      decltype(sig)& Z{sig};

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      Sign(const llarp::SecretKey& sk);

      bool
      Verify(const llarp::PubKey& pk) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      void
      Clear() override
      {
        IMessage::Clear();
        txid = 0;
        Y.Zero();
        Z.Zero();
      }
    };

    struct RejectExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer<16>;
      uint64_t backoff;
      std::vector<llarp::exit::Policy> rejected_policies;
      uint64_t txid;
      Nonce_t nonce;
      llarp::Signature sig;

      decltype(backoff)& B{backoff};
      decltype(rejected_policies)& R{rejected_policies};
      decltype(txid)& T{txid};
      decltype(nonce)& Y{nonce};
      decltype(sig)& Z{sig};

      void
      Clear() override
      {
        IMessage::Clear();
        backoff = 0;
        rejected_policies.clear();
        txid = 0;
        nonce.Zero();
        sig.Zero();
      }

      bool
      Sign(const llarp::SecretKey& sk);

      bool
      Verify(const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
    };

    struct UpdateExitVerifyMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer<16>;
      uint64_t txid;
      Nonce_t nounce;
      llarp::Signature sig;

      decltype(txid)& T{txid};
      decltype(nounce)& Y{nounce};
      decltype(sig)& Z{sig};

      ~UpdateExitVerifyMessage() override = default;

      void
      Clear() override
      {
        T = 0;
        Y.Zero();
        Z.Zero();
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
    };

    struct UpdateExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer<16>;
      llarp::PathID_t path_id;
      uint64_t txid;
      Nonce_t nounce;
      llarp::Signature sig;

      decltype(path_id)& P{path_id};
      decltype(txid)& T{txid};
      decltype(nounce)& Y{nounce};
      decltype(sig)& Z{sig};

      bool
      Sign(const llarp::SecretKey& sk);

      bool
      Verify(const llarp::PubKey& pk) const;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      void
      Clear() override
      {
        IMessage::Clear();
        P.Zero();
        T = 0;
        Y.Zero();
        Z.Zero();
      }
    };

    struct CloseExitMessage final : public IMessage
    {
      using Nonce_t = llarp::AlignedBuffer<16>;

      Nonce_t nounce;
      llarp::Signature sig;
      decltype(nounce)& Y{nounce};
      decltype(sig)& Z{sig};

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      bool
      Sign(const llarp::SecretKey& sk);

      bool
      Verify(const llarp::PubKey& pk) const;

      void
      Clear() override
      {
        IMessage::Clear();
        Y.Zero();
        Z.Zero();
      }
    };

  }  // namespace routing
}  // namespace llarp

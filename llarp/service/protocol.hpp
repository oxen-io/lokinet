#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/routing/message.hpp>
#include "protocol_type.hpp"
#include "identity.hpp"
#include "info.hpp"
#include "intro.hpp"
#include "handler.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/time.hpp>
#include <llarp/path/pathset.hpp>

#include <vector>

struct llarp_threadpool;

namespace llarp
{
  namespace path
  {
    /// forward declare
    struct Path;
  }  // namespace path

  namespace service
  {
    struct Endpoint;

    constexpr std::size_t MAX_PROTOCOL_MESSAGE_SIZE = 2048 * 2;

    /*  Note: Talk to Tom and Jason about switching the names of ProtocolFrameMessage (carrier
        object) and ProtocolMessage (inner object) to something like ProtocolMessageCarrier and
        ProtocolMessage?
    */

    /// inner message
    struct ProtocolMessage
    {
      ProtocolMessage(const ConvoTag& tag);
      ProtocolMessage();
      ~ProtocolMessage();
      ProtocolType proto = ProtocolType::TrafficV4;
      llarp_time_t queued = 0s;
      std::vector<byte_t> payload;  // encrypted AbstractLinkMessage
      Introduction introReply;
      ServiceInfo sender;
      Endpoint* handler = nullptr;
      ConvoTag tag;
      uint64_t seqno = 0;
      uint64_t version = llarp::constants::proto_version;

      /// encode metainfo for lmq endpoint auth
      std::vector<char>
      EncodeAuthInfo() const;

      bool
      decode_key(const llarp_buffer_t& key, llarp_buffer_t* val);

      std::string
      bt_encode() const;

      void
      PutBuffer(const llarp_buffer_t& payload);

      static void
      ProcessAsync(path::Path_ptr p, PathID_t from, std::shared_ptr<ProtocolMessage> self);

      bool
      operator>(const ProtocolMessage& other) const
      {
        return seqno > other.seqno;
      }
    };

    /// outer message
    struct ProtocolFrameMessage final : public routing::AbstractRoutingMessage
    {
      PQCipherBlock cipher;
      Encrypted<2048> enc;
      uint64_t flag;  // set to indicate in plaintext a nack, aka "dont try again"
      KeyExchangeNonce nonce;
      Signature sig;
      PathID_t path_id;
      service::ConvoTag convo_tag;

      ProtocolFrameMessage(const ProtocolFrameMessage& other)
          : routing::AbstractRoutingMessage(other)
          , cipher(other.cipher)
          , enc(other.enc)
          , flag(other.flag)
          , nonce(other.nonce)
          , sig(other.sig)
          , path_id(other.path_id)
          , convo_tag(other.convo_tag)
      {
        sequence_number = other.sequence_number;
        version = other.version;
      }

      ProtocolFrameMessage() : routing::AbstractRoutingMessage{}
      {
        clear();
      }

      ~ProtocolFrameMessage() override;

      bool
      operator==(const ProtocolFrameMessage& other) const;

      bool
      operator!=(const ProtocolFrameMessage& other) const
      {
        return !(*this == other);
      }

      ProtocolFrameMessage&
      operator=(const ProtocolFrameMessage& other);

      bool
      EncryptAndSign(
          const ProtocolMessage& msg, const SharedSecret& sharedkey, const Identity& localIdent);

      bool
      Sign(const Identity& localIdent);

      bool
      AsyncDecryptAndVerify(
          EventLoop_ptr loop,
          path::Path_ptr fromPath,
          const Identity& localIdent,
          Endpoint* handler,
          std::function<void(std::shared_ptr<ProtocolMessage>)> hook = nullptr) const;

      bool
      DecryptPayloadInto(const SharedSecret& sharedkey, ProtocolMessage& into) const;

      bool
      decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      /** Note: this method needs to be re-examined where it is called in the other class methods,
          like ::Sign(), ::EncryptAndSign(), and ::Verify(). In all 3 of these cases, the subsequent
          methods that the llarp_buffer_t is passed to must be refactored to take either a string, a
          redesigned llarp_buffer, or some span backport.
      */
      std::string
      bt_encode() const override;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        return bencode_decode_dict(*this, buf);
      }

      void
      clear() override
      {
        cipher.Zero();
        enc.Clear();
        path_id.Zero();
        convo_tag.Zero();
        nonce.Zero();
        sig.Zero();
        flag = 0;
        version = llarp::constants::proto_version;
      }

      bool
      Verify(const ServiceInfo& from) const;

      bool
      handle_message(routing::AbstractRoutingMessageHandler* h, Router* r) const override;
    };
  }  // namespace service
}  // namespace llarp

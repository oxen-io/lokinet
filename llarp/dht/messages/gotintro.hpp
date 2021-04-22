#pragma once

#include <llarp/dht/message.hpp>
#include <llarp/service/intro_set.hpp>
#include <llarp/util/copy_or_nullptr.hpp>

#include <vector>
#include <optional>

namespace llarp
{
  namespace dht
  {
    /// acknowledgement to PublishIntroMessage or reply to FindIntroMessage
    struct GotIntroMessage : public IMessage
    {
      /// the found introsets
      std::vector<service::EncryptedIntroSet> found;
      /// txid
      uint64_t txid = 0;
      /// the key of a router closer in keyspace if iterative lookup
      std::optional<Key_t> closer;

      GotIntroMessage(const Key_t& from) : IMessage(from)
      {}

      GotIntroMessage(const GotIntroMessage& other)
          : IMessage(other.From), found(other.found), txid(other.txid), closer(other.closer)
      {
        version = other.version;
      }

      /// for iterative reply
      GotIntroMessage(const Key_t& from, const Key_t& _closer, uint64_t _txid)
          : IMessage(from), txid(_txid), closer(_closer)
      {}

      /// for recursive reply
      GotIntroMessage(std::vector<service::EncryptedIntroSet> results, uint64_t txid);

      ~GotIntroMessage() override = default;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(llarp_dht_context* ctx, std::vector<IMessage::Ptr_t>& replies) const override;
    };

    struct RelayedGotIntroMessage final : public GotIntroMessage
    {
      RelayedGotIntroMessage() : GotIntroMessage({})
      {}

      bool
      HandleMessage(llarp_dht_context* ctx, std::vector<IMessage::Ptr_t>& replies) const override;
    };

    using GotIntroMessage_constptr = std::shared_ptr<const GotIntroMessage>;
  }  // namespace dht
}  // namespace llarp

#pragma once

#include "dht.h"
#include "key.hpp"
#include <llarp/path/path_types.hpp>
#include <llarp/util/bencode.hpp>

#include <vector>

namespace llarp
{
  namespace dht
  {
    constexpr size_t MAX_MSG_SIZE = 2048;

    struct IMessage
    {
      virtual ~IMessage() = default;

      /// construct
      IMessage(const Key_t& from) : From(from)
      {}

      using Ptr_t = std::unique_ptr<IMessage>;

      virtual bool
      HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const = 0;

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) = 0;

      Key_t From;
      PathID_t pathID;
      uint64_t version = LLARP_PROTO_VERSION;
    };

    IMessage::Ptr_t
    DecodeMessage(const Key_t& from, llarp_buffer_t* buf, bool relayed = false);

    bool
    DecodeMesssageList(
        Key_t from, llarp_buffer_t* buf, std::vector<IMessage::Ptr_t>& dst, bool relayed = false);
  }  // namespace dht
}  // namespace llarp

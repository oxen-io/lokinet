#pragma once

#include <llarp/messages/common.hpp>
#include <llarp/constants/proto.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/buffer.hpp>

namespace
{
  static auto route_cat = llarp::log::Cat("lokinet.routing");
}  // namespace

namespace llarp
{
  struct Router;
  namespace routing
  {
    struct AbstractRoutingMessageHandler;

    struct AbstractRoutingMessage : private AbstractSerializable
    {
      PathID_t from;
      uint64_t sequence_number{0};
      uint64_t version = llarp::constants::proto_version;

      AbstractRoutingMessage() = default;

      virtual ~AbstractRoutingMessage() = default;

      virtual bool
      decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) = 0;

      std::string
      bt_encode() const override = 0;

      virtual bool
      handle_message(AbstractRoutingMessageHandler* h, Router* r) const = 0;

      virtual void
      clear() = 0;

      // methods we do not want to inherit onwards from AbstractSerializable
      void
      bt_encode(oxenc::bt_list_producer&) const final
      {
        throw std::runtime_error{
            "Error: Routing messages should not encode directly to a bt list producer!"};
      }
      void
      bt_encode(llarp_buffer&) const final
      {
        throw std::runtime_error{"Error: Routing messages should not encode directly to a buffer!"};
      }
      void
      bt_encode(oxenc::bt_dict_producer&) const final
      {
        throw std::runtime_error{
            "Error: Routing messages should not encode directly to a bt dict producer!"};
      }

      bool
      operator<(const AbstractRoutingMessage& other) const
      {
        return other.sequence_number < sequence_number;
      }
    };

  }  // namespace routing
}  // namespace llarp

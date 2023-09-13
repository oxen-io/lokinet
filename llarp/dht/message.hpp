#pragma once

#include "dht.h"
#include "key.hpp"
#include <llarp/messages/link_message.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/bencode.hpp>

#include <vector>

namespace
{
  static auto dht_cat = llarp::log::Cat("lokinet.dht");
}  // namespace

namespace llarp::dht
{
  constexpr size_t MAX_MSG_SIZE = 2048;

  struct AbstractDHTMessageHandler;

  struct AbstractDHTMessage : private AbstractSerializable
  {
    virtual ~AbstractDHTMessage() = default;

    /// construct
    AbstractDHTMessage(const Key_t& from) : From(from)
    {}

    virtual bool
    handle_message(
        AbstractDHTMessageHandler& dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const = 0;

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override = 0;

    virtual bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) = 0;

    // methods we do not want to inherit onwards from AbstractSerializable
    void
    bt_encode(oxenc::bt_list_producer&) const final
    {
      throw std::runtime_error{"Error: DHT messages should encode directly to a bt dict producer!"};
    }
    void
    bt_encode(llarp_buffer&) const final
    {
      throw std::runtime_error{"Error: DHT messages should encode directly to a bt dict producer!"};
    }
    std::string
    bt_encode() const final
    {
      throw std::runtime_error{"Error: DHT messages should encode directly to a bt dict producer!"};
    }

    Key_t From;
    PathID_t pathID;
    uint64_t version = llarp::constants::proto_version;
  };

  std::unique_ptr<AbstractDHTMessage>
  DecodeMessage(const Key_t& from, llarp_buffer_t* buf, bool relayed = false);

  bool
  DecodeMessageList(
      Key_t from,
      llarp_buffer_t* buf,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& list,
      bool relayed = false);

}  // namespace llarp::dht

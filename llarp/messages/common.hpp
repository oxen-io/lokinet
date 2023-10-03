#pragma once

#include <llarp/dht/key.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/service/tag.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>

#include <oxenc/bt.h>

namespace
{
  static auto link_cat = llarp::log::Cat("lokinet.link");
}  // namespace

namespace llarp
{
  /// abstract base class for serialized messages
  struct AbstractSerializable
  {
    virtual std::string
    bt_encode() const = 0;
    virtual void
    bt_encode(oxenc::bt_dict_producer& btdp) const = 0;
  };

  struct AbstractMessageHandler
  {
    virtual bool
    handle_message(AbstractSerializable&) = 0;
  };
}  // namespace llarp

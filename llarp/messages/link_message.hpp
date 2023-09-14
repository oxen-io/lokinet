#pragma once

#include "common.hpp"

#include <llarp/link/session.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct AbstractLinkSession;
  struct AbstractRouter;

  /// parsed link layer message
  struct AbstractLinkMessage : private AbstractSerializable
  {
    /// who did this message come from or is going to
    AbstractLinkSession* session = nullptr;
    uint64_t version = llarp::constants::proto_version;

    PathID_t pathid;

    AbstractLinkMessage() = default;

    virtual ~AbstractLinkMessage() = default;

    std::string
    bt_encode() const override = 0;

    virtual bool
    handle_message(AbstractRouter* router) const = 0;

    virtual bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) = 0;

    virtual void
    clear() = 0;

    // the name of this kind of message
    virtual const char*
    name() const = 0;

    /// get message prority, higher value means more important
    virtual uint16_t
    priority() const
    {
      return 1;
    }

    // methods we do not want to inherit onwards from AbstractSerializable
    void
    bt_encode(llarp_buffer&) const final
    {
      throw std::runtime_error{"Error: Link messages should not encode directly to a buffer!"};
    }
    void
    bt_encode(oxenc::bt_dict_producer&) const final
    {
      throw std::runtime_error{
          "Error: Link messages should not encode directly to a bt list producer!"};
    }
    void
    bt_encode(oxenc::bt_list_producer&) const final
    {
      throw std::runtime_error{
          "Error: Link messages should not encode directly to a bt list producer!"};
    }
  };

}  // namespace llarp

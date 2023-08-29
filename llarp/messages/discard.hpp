#pragma once

#include "link_message.hpp"
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/util/bencode.hpp>

namespace llarp
{
  struct DiscardMessage final : public AbstractLinkMessage
  {
    DiscardMessage() : AbstractLinkMessage()
    {}

    std::string
    bt_encode() const override
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("a", "x");
      }
      catch (...)
      {
        log::critical(link_cat, "Error: RelayDownstreamMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }

    void
    clear() override
    {
      version = 0;
    }

    const char*
    name() const override
    {
      return "Discard";
    }

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override
    {
      if (key.startswith("a"))
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(buf, &strbuf))
          return false;
        if (strbuf.sz != 1)
          return false;
        return *strbuf.cur == 'x';
      }
      return false;
    }

    bool
    handle_message(AbstractRouter* /*router*/) const override
    {
      return true;
    }
  };

  namespace routing
  {
    struct DataDiscardMessage final : public AbstractRoutingMessage
    {
      PathID_t P;

      DataDiscardMessage() = default;

      DataDiscardMessage(const PathID_t& dst, uint64_t s) : P(dst)
      {
        S = s;
        version = llarp::constants::proto_version;
      }

      void
      clear() override
      {
        version = 0;
      }

      bool
      handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const override
      {
        return h->HandleDataDiscardMessage(*this, r);
      }

      bool
      decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf) override
      {
        bool read = false;
        if (!BEncodeMaybeReadDictEntry("P", P, read, k, buf))
          return false;
        if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
          return false;
        if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
          return false;
        return read;
      }

      std::string
      bt_encode() const override
      {
        oxenc::bt_dict_producer btdp;

        try
        {
          btdp.append("A", "D");
          btdp.append("P", P.ToView());
          btdp.append("S", S);
          btdp.append("V", version);
        }
        catch (...)
        {
          log::critical(route_cat, "Error: DataDiscardMessage failed to bt encode contents!");
        }

        return std::move(btdp).str();
      }
    };
  }  // namespace routing

}  // namespace llarp

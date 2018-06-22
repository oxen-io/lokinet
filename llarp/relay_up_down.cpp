#include <llarp/bencode.hpp>
#include <llarp/messages/relay.hpp>

#include "router.hpp"

namespace llarp
{
  RelayUpstreamMessage::RelayUpstreamMessage(const RouterID &from)
      : ILinkMessage(from)
  {
  }

  RelayUpstreamMessage::RelayUpstreamMessage() : ILinkMessage()
  {
  }

  RelayUpstreamMessage::~RelayUpstreamMessage()
  {
  }

  bool
  RelayUpstreamMessage::BEncode(llarp_buffer_t *buf) const
  {
    // TODO: implement me
    return false;
  }

  bool
  RelayUpstreamMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    return false;
  }

  bool
  RelayUpstreamMessage::HandleMessage(llarp_router *router) const
  {
    return false;
  }

  RelayDownstreamMessage::RelayDownstreamMessage(const RouterID &from)
      : ILinkMessage(from)
  {
  }

  RelayDownstreamMessage::RelayDownstreamMessage() : ILinkMessage()
  {
  }

  RelayDownstreamMessage::~RelayDownstreamMessage()
  {
  }
  bool
  RelayDownstreamMessage::BEncode(llarp_buffer_t *buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    if(!BEncodeWriteDictMsgType(buf, "a", "d"))
      return false;

    if(!BEncodeWriteDictEntry("p", pathid, buf))
      return false;
    if(!BEncodeWriteDictInt(buf, "v", LLARP_PROTO_VERSION))
      return false;
    if(!BEncodeWriteDictEntry("x", X, buf))
      return false;
    if(!BEncodeWriteDictEntry("y", Y, buf))
      return false;
    return bencode_end(buf);
  }

  bool
  RelayDownstreamMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    bool read = false;
    if(!BEncodeMaybeReadDictEntry("p", pathid, read, key, buf))
      return false;
    if(!BEncodeMaybeReadVersion("v", version, LLARP_PROTO_VERSION, read, key,
                                buf))
      return false;
    if(!BEncodeMaybeReadDictEntry("x", X, read, key, buf))
      return false;
    if(!BEncodeMaybeReadDictEntry("y", Y, read, key, buf))
      return false;
    return read;
  }

  bool
  RelayDownstreamMessage::HandleMessage(llarp_router *router) const
  {
    PathID_t id    = pathid;
    id.data_l()[0] = 0;
    auto path      = router->paths.GetByUpstream(remote, id);
    if(path)
    {
      return path->HandleDownstream(X.Buffer(), Y, router);
    }
    else
    {
      llarp::Warn("No such path upstream=", remote, " pathid=", id);
    }
    return false;
  }
}  // namespace llarp

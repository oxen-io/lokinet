#include <messages/relay.hpp>
#include <router.hpp>
#include <util/bencode.hpp>

namespace llarp
{
  RelayUpstreamMessage::RelayUpstreamMessage() : ILinkMessage()
  {
  }

  RelayUpstreamMessage::~RelayUpstreamMessage()
  {
  }

  void
  RelayUpstreamMessage::Clear()
  {
    pathid.Zero();
    X.Clear();
    Y.Zero();
  }

  bool
  RelayUpstreamMessage::BEncode(llarp_buffer_t *buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    if(!BEncodeWriteDictMsgType(buf, "a", "u"))
      return false;

    if(!BEncodeWriteDictEntry("p", pathid, buf))
      return false;
    if(!BEncodeWriteDictInt("v", LLARP_PROTO_VERSION, buf))
      return false;
    if(!BEncodeWriteDictEntry("x", X, buf))
      return false;
    if(!BEncodeWriteDictEntry("y", Y, buf))
      return false;
    return bencode_end(buf);
  }

  bool
  RelayUpstreamMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
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
  RelayUpstreamMessage::HandleMessage(llarp::Router *r) const
  {
    auto path = r->paths.GetByDownstream(session->GetPubKey(), pathid);
    if(path)
    {
      return path->HandleUpstream(X.Buffer(), Y, r);
    }
    return false;
  }

  RelayDownstreamMessage::RelayDownstreamMessage() : ILinkMessage()
  {
  }

  RelayDownstreamMessage::~RelayDownstreamMessage()
  {
  }

  void
  RelayDownstreamMessage::Clear()
  {
    pathid.Zero();
    X.Clear();
    Y.Zero();
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
    if(!BEncodeWriteDictInt("v", LLARP_PROTO_VERSION, buf))
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
  RelayDownstreamMessage::HandleMessage(llarp::Router *r) const
  {
    auto path = r->paths.GetByUpstream(session->GetPubKey(), pathid);
    if(path)
    {
      return path->HandleDownstream(X.Buffer(), Y, r);
    }
    llarp::LogWarn("unhandled downstream message");
    return false;
  }
}  // namespace llarp

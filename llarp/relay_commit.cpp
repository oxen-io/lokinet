#include <llarp/bencode.hpp>
#include <llarp/messages/relay_commit.hpp>
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  LR_CommitMessage::~LR_CommitMessage()
  {
  }

  bool
  LR_CommitMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "c"))
    {
      return BEncodeReadList(frames, buf);
    }
    if(llarp_buffer_eq(key, "f"))
    {
      return lasthopFrame.BDecode(buf);
    }
    if(llarp_buffer_eq(key, "r"))
    {
      return BEncodeReadList(acks, buf);
    }
    bool read = false;
    if(!BEncodeMaybeReadVersion("v", version, LLARP_PROTO_VERSION, read, key,
                                buf))
      return false;

    return read;
  }

  bool
  LR_CommitMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    // msg type
    if(!BEncodeWriteDictMsgType(buf, "a", "c"))
      return false;
    // frames
    if(!BEncodeWriteDictList("c", frames, buf))
      return false;
    // last hop
    if(!BEncodeWriteDictEntry("f", lasthopFrame, buf))
      return false;
    // acks
    if(!BEncodeWriteDictList("r", acks, buf))
      return false;
    // version
    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitMessage::HandleMessage(llarp_router* router) const
  {
    return router->paths.HandleRelayCommit(this);
  }

  bool
  LR_CommitRecord::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!BEncodeWriteDictEntry("c", commkey, buf))
      return false;
    if(!BEncodeWriteDictEntry("i", nextHop, buf))
      return false;
    if(!BEncodeWriteDictEntry("n", tunnelNonce, buf))
      return false;
    if(!BEncodeWriteDictEntry("p", txid, buf))
      return false;
    if(!BEncodeWriteDictEntry("s", downstreamReplyKey, buf))
      return false;
    if(!BEncodeWriteDictEntry("u", downstreamReplyNonce, buf))
      return false;
    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitRecord::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    if(!key)
      return true;

    LR_CommitRecord* self = static_cast< LR_CommitRecord* >(r->user);

    bool read = false;

    if(!BEncodeMaybeReadDictEntry("c", self->commkey, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("i", self->nextHop, read, *key, r->buffer))
      return false;
    if(BEncodeMaybeReadDictEntry("n", self->tunnelNonce, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("p", self->txid, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("s", self->downstreamReplyKey, read, *key,
                                  r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("u", self->downstreamReplyNonce, read, *key,
                                  r->buffer))
      return false;
    if(!BEncodeMaybeReadVersion("v", self->version, LLARP_PROTO_VERSION, read,
                                *key, r->buffer))
      return false;

    return read;
  }

  bool
  LR_CommitRecord::BDecode(llarp_buffer_t* buf)
  {
    dict_reader r;
    r.user   = this;
    r.on_key = &OnKey;
    return bencode_read_dict(buf, &r);
  }
}

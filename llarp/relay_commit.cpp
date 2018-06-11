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
      return DecodeEncryptedFrameList(buf);
    }
    return false;
  }

  bool
  LR_CommitMessage::DecodeEncryptedFrame(list_reader* l, bool item)
  {
    LR_CommitMessage* self = static_cast< LR_CommitMessage* >(l->user);
    // end of list
    if(!item)
    {
      bool success = self->frames.size() > 0;
      if(!success)
        llarp::Error("no encrypted frames read in LRCM");
      return success;
    }

    llarp_buffer_t strbuf;
    if(!bencode_read_string(l->buffer, &strbuf))
      return false;

    if(strbuf.sz <= EncryptedFrame::OverheadSize)
    {
      llarp::Error("Encrypted Frame In LRCM too short, ", strbuf.sz,
                   " <= ", EncryptedFrame::OverheadSize);
      return false;
    }
    // emplace new frame
    self->frames.emplace_back(strbuf.cur, strbuf.sz);
    return true;
  }

  bool
  LR_CommitMessage::DecodeEncryptedFrameList(llarp_buffer_t* buf)
  {
    list_reader r;
    r.user    = this;
    r.on_item = &DecodeEncryptedFrame;
    return bencode_read_list(buf, &r);
  }

  bool
  LR_CommitMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    // msg type
    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "c", 1))
      return false;

    // frames
    if(!bencode_write_bytestring(buf, "c", 1))
      return false;
    if(!bencode_start_list(buf))
      return false;
    for(const auto& frame : frames)
      if(!bencode_write_bytestring(buf, frame.data, frame.size))
        return false;
    if(!bencode_end(buf))
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
    return false;
  }

  bool
  LR_CommitRecord::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    if(!key)
      return true;

    llarp_buffer_t strbuf;

    LR_CommitRecord* self = static_cast< LR_CommitRecord* >(r->user);
    if(llarp_buffer_eq(*key, "c"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != sizeof(llarp_pubkey_t))
        return false;
      memcpy(self->commkey, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "i"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != sizeof(llarp_pubkey_t))
        return false;
      memcpy(self->nextHop, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "n"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != sizeof(llarp_tunnel_nonce_t))
        return false;
      memcpy(self->tunnelNonce, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "p"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != self->txid.size())
        return false;
      memcpy(self->txid, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "s"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != self->downstreamReplyKey.size())
        return false;
      memcpy(self->downstreamReplyKey, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "u"))
    {
      if(!bencode_read_string(r->buffer, &strbuf))
        return false;
      if(strbuf.sz != self->downstreamReplyNonce.size())
        return false;
      memcpy(self->downstreamReplyNonce, strbuf.base, strbuf.sz);
      return true;
    }

    if(llarp_buffer_eq(*key, "v"))
    {
      if(!bencode_read_integer(r->buffer, &self->version))
        return false;
      return self->version == LLARP_PROTO_VERSION;
    }

    return false;
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

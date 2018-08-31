#ifdef _MSC_VER
#define NOMINMAX
#endif

#include "llarp/iwp/frame_state.hpp"
#include <algorithm>
#include "buffer.hpp"
#include "llarp/crypto.hpp"
#include "llarp/endian.h"
#include "llarp/iwp/inbound_message.hpp"
#include "llarp/iwp/session.hpp"
#include "llarp/logger.hpp"
#include "llarp/iwp/server.hpp"
#include "mem.hpp"
#include "router.hpp"

llarp_router *
frame_state::Router()
{
  return parent->Router();
}

bool
frame_state::process_inbound_queue()
{
  uint64_t last = rxids;
  recvqueue.Process([&](InboundMessage &msg) {
    if(last != msg.msgid)
    {
      auto buffer = msg.Buffer();
      if(!Router()->HandleRecvLinkMessage(parent, buffer))
      {
        llarp::LogWarn("failed to process inbound message ", msg.msgid);
        llarp::DumpBuffer< llarp_buffer_t, 128 >(buffer);
      }
      last = msg.msgid;
    }
    else
    {
      llarp::LogWarn("duplicate inbound message ", last);
    }
  });
  // TODO: this isn't right
  return true;
}

bool
frame_state::flags_agree(byte_t flags) const
{
  return ((rxflags & flags) & (txflags & flags)) == flags;
}

bool
frame_state::either_has_flag(byte_t flag) const
{
  return (rxflags & flag) == flag || (txflags & flag) == flag;
}

void
frame_state::clear()
{
  rx.clear();
  tx.clear();
}

bool
frame_state::got_xmit(frame_header hdr, size_t sz)
{
  if(hdr.size() > sz)
  {
    // overflow
    llarp::LogWarn("invalid XMIT frame size ", hdr.size(), " > ", sz);
    return false;
  }
  sz = hdr.size();

  // extract xmit data
  xmit x(hdr.data());

  const auto bufsz = sizeof(x.buffer);

  if(sz - bufsz < x.lastfrag())
  {
    // bad size of last fragment
    llarp::LogWarn("XMIT frag size missmatch ", sz - bufsz, " < ",
                   x.lastfrag());
    return false;
  }

  // check LSB set on flags
  if(x.flags() & 0x01)
  {
    auto id  = x.msgid();
    auto h   = x.hash();
    auto itr = rx.find(h);
    if(itr == rx.end())
    {
      if(x.numfrags() > 0)
      {
        auto &msg = rx.insert(std::make_pair(h, transit_message(x)))

                        .first->second;
        rxIDs.insert(std::make_pair(id, h));
        llarp::LogDebug("got message XMIT with ", (int)x.numfrags(),
                        " fragment"
                        "s");
        // inserted, put last fragment
        msg.put_lastfrag(hdr.data() + sizeof(x.buffer), x.lastfrag());
        push_ackfor(id, 0);
        return true;
      }
      else
      {
        // handle zero fragment message immediately
        transit_message msg(x);
        msg.put_lastfrag(hdr.data() + sizeof(x.buffer), x.lastfrag());
        push_ackfor(id, 0);
        return inbound_frame_complete(msg);
      }
    }
    else
      llarp::LogWarn("duplicate XMIT h=", llarp::ShortHash(h));
  }
  else
    llarp::LogWarn("LSB not set on flags");
  return false;
}

bool
frame_state::got_frag(frame_header hdr, size_t sz)
{
  if(hdr.size() > sz)
  {
    // overflow
    llarp::LogWarn("invalid FRAG frame size ", hdr.size(), " > ", sz);
    return false;
  }
  sz = hdr.size();

  if(sz <= 9)
  {
    // underflow
    llarp::LogWarn("invalid FRAG frame size ", sz, " <= 9");
    return false;
  }

  uint64_t msgid;
  byte_t fragno;
  msgid      = bufbe64toh(hdr.data());
  fragno     = hdr.data()[8];
  auto idItr = rxIDs.find(msgid);
  if(idItr == rxIDs.end())
  {
    push_ackfor(msgid, ~0);
    return true;
  }
  auto itr = rx.find(idItr->second);
  if(itr == rx.end())
  {
    push_ackfor(msgid, ~0);
    return true;
  }
  auto fragsize = itr->second.msginfo.fragsize();
  if(fragsize != sz - 9)
  {
    llarp::LogWarn("RX fragment size missmatch ", fragsize, " != ", sz - 9);
    return false;
  }
  llarp::LogDebug("RX got fragment ", (int)fragno, " msgid=", msgid);
  if(!itr->second.put_frag(fragno, hdr.data() + 9))
  {
    llarp::LogWarn("inbound message does not have fragment msgid=", msgid,
                   " fragno=", (int)fragno);
    return false;
  }
  auto mask = itr->second.get_bitmask();
  if(itr->second.completed())
  {
    push_ackfor(msgid, mask);
    bool result = inbound_frame_complete(itr->second);
    rxIDs.erase(idItr);
    rx.erase(itr);
    return result;
  }
  else if(itr->second.should_send_ack(llarp_time_now_ms()))
  {
    push_ackfor(msgid, mask);
  }
  return true;
}

void
frame_state::push_ackfor(uint64_t id, uint32_t bitmask)
{
  llarp::LogDebug("ACK for msgid=", id, " mask=", bitmask);
  sendqueue.EmplaceIf(
      [&](sendbuf_t &pkt) -> bool {
        auto body_ptr = init_sendbuf(&pkt, eACKS, 12, txflags);
        htobe64buf(body_ptr, id);
        htobe32buf(body_ptr + 8, bitmask);
        return true;
      },
      18);
}

bool
frame_state::inbound_frame_complete(const transit_message &rxmsg)
{
  bool success = false;
  std::vector< byte_t > msg;
  llarp::ShortHash digest;

  auto id = rxmsg.msginfo.msgid();

  if(rxmsg.reassemble(msg))
  {
    auto router = Router();
    auto buf    = llarp::Buffer< decltype(msg) >(msg);
    router->crypto.shorthash(digest, buf);
    if(memcmp(digest, rxmsg.msginfo.hash(), 32))
    {
      llarp::LogWarn("message hash missmatch ", digest,
                     " != ", llarp::AlignedBuffer< 32 >(rxmsg.msginfo.hash()));
      return false;
    }

    llarp_link_session *impl = parent;

    if(id == 0)
    {
      success = router->HandleRecvLinkMessage(parent, buf);
      if(impl->CheckRCValid())
      {
        if(!impl->IsEstablished())
        {
          // client side got server's LIM
          impl->send_LIM();
          impl->session_established();
        }
        ++nextMsgID;
      }
      else
      {
        llarp::PubKey k = impl->remote_router.pubkey;
        llarp::LogWarn("spoofed LIM from ", k);
        impl->close();
        success = false;
      }
    }
    else
    {
      recvqueue.Emplace(id, msg);
      success = true;
    }
  }

  if(!success)
    llarp::LogWarn("Failed to process inbound message ", id);

  return success;
}

bool
frame_state::got_acks(frame_header hdr, size_t sz)
{
  if(hdr.size() > sz)
  {
    llarp::LogError("invalid ACKS frame size ", hdr.size(), " > ", sz);
    return false;
  }
  sz = hdr.size();
  if(sz < 12)
  {
    llarp::LogError("invalid ACKS frame size ", sz, " < 12");
    return false;
  }

  auto ptr         = hdr.data();
  uint64_t msgid   = bufbe64toh(ptr);
  uint32_t bitmask = bufbe32toh(ptr + 8);

  auto itr = tx.find(msgid);
  if(itr == tx.end())
  {
    llarp::LogDebug("ACK for missing TX frame msgid=", msgid);
    return true;
  }

  auto now = llarp_time_now_ms();

  if(bitmask == ~(0U))
  {
    tx.erase(msgid);
  }
  else
  {
    itr->second.ack(bitmask);

    if(itr->second.completed())
    {
      llarp::LogDebug("message transmitted msgid=", msgid);
      tx.erase(itr);
    }
    else if(itr->second.should_resend_frags(now))
    {
      llarp::LogDebug("message ", msgid, " retransmit fragments");
      itr->second.retransmit_frags(sendqueue, txflags);
    }
  }
  return true;
}

bool
frame_state::process(byte_t *buf, size_t sz)
{
  frame_header hdr(buf);
  if(hdr.flags() & eSessionInvalidated)
  {
    rxflags |= eSessionInvalidated;
  }
  switch(hdr.msgtype())
  {
    case eALIV:
      // llarp::LogDebug("iwp_link::frame_state::process Got alive");
      if(rxflags & eSessionInvalidated)
      {
        txflags |= eSessionInvalidated;
      }
      return true;
    case eXMIT:
      llarp::LogDebug("iwp_link::frame_state::process Got xmit");
      return got_xmit(hdr, sz - 6);
    case eACKS:
      llarp::LogDebug("iwp_link::frame_state::process Got ack");
      return got_acks(hdr, sz - 6);
    case msgtype::eFRAG:
      llarp::LogDebug("iwp_link::frame_state::process Got frag");
      return got_frag(hdr, sz - 6);
    default:
      llarp::LogWarn(
          "iwp_link::frame_state::process - unknown header message type: ",
          (int)hdr.msgtype());
      return false;
  }
}

void
frame_state::retransmit(llarp_time_t now)
{
  for(auto &item : tx)
  {
    if(item.second.should_resend_xmit(now))
    {
      item.second.generate_xmit(sendqueue, txflags);
    }
    item.second.retransmit_frags(sendqueue, txflags);
  }
}

void
frame_state::alive()
{
  lastEvent = llarp_time_now_ms();
}

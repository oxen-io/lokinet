#include "llarp/iwp/frame_state.hpp"
#include "llarp/iwp/inbound_message.hpp"
#include "llarp/iwp/session.hpp"

#include "buffer.hpp"
#include "llarp/crypto.hpp"
#include "llarp/logger.hpp"
#include "router.hpp"

llarp_router *
frame_state::Router()
{
  return parent->Router();
}

bool
frame_state::process_inbound_queue()
{
  std::priority_queue< InboundMessage *, std::vector< InboundMessage * >,
                       InboundMessage::OrderCompare >
      q;
  recvqueue.Process(q);
  bool increment = false;
  while(q.size())
  {
    // TODO: is this right?
    auto &front = q.top();
    // the items are already sorted anyways so this doesn't really do much
    nextMsgID = std::max(nextMsgID, front->msgid);
    if(!Router()->HandleRecvLinkMessage(parent, front->Buffer()))
    {
      llarp::LogWarn("failed to process inbound message ", front->msgid);
    }
    delete front;
    q.pop();
    increment = true;
  }
  if(increment)
    ++nextMsgID;
  // TODO: this isn't right
  return true;
}

bool
frame_state::flags_agree(byte_t flags) const
{
  return ((rxflags & flags) & (txflags & flags)) == flags;
}

void
frame_state::clear()
{
  auto _rx = rx;
  auto _tx = tx;
  for(auto &item : _rx)
    delete item.second;
  for(auto &item : _tx)
    delete item.second;
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
    auto itr = rx.find(id);
    if(itr == rx.end())
    {
      auto msg = new transit_message(x);
      rx[id]   = msg;
      llarp::LogDebug("got message XMIT with ", (int)x.numfrags(),
                      " fragments");
      // inserted, put last fragment
      msg->put_lastfrag(hdr.data() + sizeof(x.buffer), x.lastfrag());
      push_ackfor(id, 0);
      if(x.numfrags() == 0)
      {
        return inbound_frame_complete(id);
      }
      return true;
    }
    else
      llarp::LogWarn("duplicate XMIT msgid=", x.msgid());
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
  // assumes big endian
  // TODO: implement little endian
  memcpy(&msgid, hdr.data(), 8);
  memcpy(&fragno, hdr.data() + 8, 1);

  auto itr = rx.find(msgid);
  if(itr == rx.end())
  {
    llarp::LogWarn("no such RX fragment, msgid=", msgid);
    return true;
  }
  auto fragsize = itr->second->msginfo.fragsize();
  if(fragsize != sz - 9)
  {
    llarp::LogWarn("RX fragment size missmatch ", fragsize, " != ", sz - 9);
    return false;
  }
  llarp::LogDebug("RX got fragment ", (int)fragno, " msgid=", msgid);
  if(!itr->second->put_frag(fragno, hdr.data() + 9))
  {
    llarp::LogWarn("inbound message does not have fragment msgid=", msgid,
                   " fragno=", (int)fragno);
    return false;
  }
  auto mask = itr->second->get_bitmask();
  if(itr->second->completed())
  {
    push_ackfor(msgid, mask);
    return inbound_frame_complete(msgid);
  }
  else if(itr->second->should_send_ack(llarp_time_now_ms()))
  {
    push_ackfor(msgid, mask);
  }
  return true;
}

void
frame_state::push_ackfor(uint64_t id, uint32_t bitmask)
{
  llarp::LogDebug("ACK for msgid=", id, " mask=", bitmask);
  sendqueue.push(new sendbuf_t(12 + 6));
  auto body_ptr = init_sendbuf(sendqueue.back(), eACKS, 12, txflags);
  // TODO: this assumes big endian
  memcpy(body_ptr, &id, 8);
  memcpy(body_ptr + 8, &bitmask, 4);
}

bool
frame_state::inbound_frame_complete(uint64_t id)
{
  bool success = false;
  std::vector< byte_t > msg;
  auto rxmsg = rx[id];
  if(rxmsg->reassemble(msg))
  {
    auto router = Router();
    llarp::ShortHash digest;
    auto buf = llarp::Buffer< decltype(msg) >(msg);
    router->crypto.shorthash(digest, buf);
    if(memcmp(digest, rxmsg->msginfo.hash(), 32))
    {
      llarp::LogWarn("message hash missmatch ",
                     llarp::AlignedBuffer< 32 >(digest),
                     " != ", llarp::AlignedBuffer< 32 >(rxmsg->msginfo.hash()));
      return false;
    }
    if(id == nextMsgID)
    {
      llarp_link_session *impl = parent;

      if(id == 0)
      {
        success = router->HandleRecvLinkMessage(parent, buf);
        if(impl->CheckRCValid())
        {
          if(!impl->IsEstablished())
          {
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
        recvqueue.Put(new InboundMessage(id, msg));
        success = true;
      }
    }
    else
    {
      llarp::LogWarn("out of order message expected ", nextMsgID, " but got ",
                     id);
      recvqueue.Put(new InboundMessage(id, msg));
      success = true;
    }
  }
  delete rxmsg;
  rx.erase(id);

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

  auto ptr = hdr.data();
  uint64_t msgid;
  uint32_t bitmask;
  memcpy(&msgid, ptr, 8);
  memcpy(&bitmask, ptr + 8, 4);

  auto itr = tx.find(msgid);
  if(itr == tx.end())
  {
    llarp::LogDebug("ACK for missing TX frame msgid=", msgid);
    return true;
  }

  transit_message *msg = itr->second;

  msg->ack(bitmask);

  if(msg->completed())
  {
    llarp::LogDebug("message transmitted msgid=", msgid);
    tx.erase(msgid);
    delete msg;
  }
  else
  {
    llarp::LogDebug("message ", msgid, " retransmit fragments");
    msg->retransmit_frags(sendqueue, txflags);
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
      llarp::LogDebug("iwp_link::frame_state::process Got alive");
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

bool
frame_state::next_frame(llarp_buffer_t *buf)
{
  auto left = sendqueue.size();
  llarp::LogDebug("next frame, ", left, " frames left in send queue");
  if(left)
  {
    sendbuf_t *send = sendqueue.front();
    buf->base       = send->data();
    buf->cur        = send->data();
    buf->sz         = send->size();
    return true;
  }
  return false;
}

void
frame_state::pop_next_frame()
{
  sendbuf_t *buf = sendqueue.front();
  sendqueue.pop();
  delete buf;
}

void
frame_state::queue_tx(uint64_t id, transit_message *msg)
{
  tx.insert(std::make_pair(id, msg));
  msg->generate_xmit(sendqueue, txflags);
}

void
frame_state::retransmit(llarp_time_t now)
{
  for(auto &item : tx)
  {
    if(item.second->should_resend_xmit(now))
    {
      item.second->generate_xmit(sendqueue, txflags);
    }
    item.second->retransmit_frags(sendqueue, txflags);
  }
}

void
frame_state::alive()
{
  lastEvent = llarp_time_now_ms();
}
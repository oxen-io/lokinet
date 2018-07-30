#include "llarp/iwp/transit_message.hpp"
#include "llarp/endian.h"
#include "llarp/iwp/frame_state.hpp"
#include "llarp/iwp/sendbuf.hpp"
#include "llarp/time.h"

void
transit_message::clear()
{
  frags.clear();
  lastfrag.clear();
}

// calculate acked bitmask
uint32_t
transit_message::get_bitmask() const
{
  uint32_t bitmask = 0;
  uint8_t idx      = 0;
  while(idx < 32)
  {
    bitmask |= (status.test(idx) ? (1 << idx) : 0);
    ++idx;
  }
  return bitmask;
}

// outbound
transit_message::transit_message(llarp_buffer_t buf, const byte_t *hash,
                                 uint64_t id, uint16_t mtu)
{
  started = llarp_time_now_ms();
  put_message(buf, hash, id, mtu);
}

// inbound
transit_message::transit_message(const xmit &x) : msginfo(x)
{
  started           = llarp_time_now_ms();
  byte_t fragidx    = 0;
  uint16_t fragsize = x.fragsize();
  while(fragidx < x.numfrags())
  {
    frags[fragidx].resize(fragsize);
    ++fragidx;
  }
  status.reset();
}

/// ack packets based off a bitmask
void
transit_message::ack(uint32_t bitmask)
{
  uint8_t idx = 0;
  while(idx < 32)
  {
    if(bitmask & (1 << idx))
    {
      status.set(idx);
    }
    ++idx;
  }
  lastAck = llarp_time_now_ms();
}

bool
transit_message::should_send_ack(llarp_time_t now) const
{
  if(now < started)
    return false;
  if(msginfo.numfrags() == 0)
    return true;
  if(status.count() == 0)
    return true;
  return now - lastRetransmit > 200;
}

bool
transit_message::should_resend_xmit(llarp_time_t now) const
{
  if(now < started)
    return false;
  return lastAck == 0 && now - started > 1000;
}

bool
transit_message::should_resend_frags(llarp_time_t now) const
{
  if(now < started || now < lastAck)
    return false;
  return lastAck > 0 && now - lastAck > 500 && !completed();
}

bool
transit_message::completed() const
{
  for(byte_t idx = 0; idx < msginfo.numfrags(); ++idx)
  {
    if(!status.test(idx))
      return false;
  }
  return true;
}

// template < typename T >
void
transit_message::generate_xmit(sendqueue_t &queue, byte_t flags)
{
  uint16_t sz   = lastfrag.size() + sizeof(msginfo.buffer);
  auto pkt      = new sendbuf_t(sz + 6);
  auto body_ptr = init_sendbuf(pkt, eXMIT, sz, flags);
  memcpy(body_ptr, msginfo.buffer, sizeof(msginfo.buffer));
  body_ptr += sizeof(msginfo.buffer);
  memcpy(body_ptr, lastfrag.data(), lastfrag.size());
  queue.Put(pkt);
}

// template < typename T >
void
transit_message::retransmit_frags(sendqueue_t &queue, byte_t flags)
{
  auto msgid    = msginfo.msgid();
  auto fragsize = msginfo.fragsize();
  for(auto &frag : frags)
  {
    if(status.test(frag.first))
      continue;
    uint16_t sz   = 9 + fragsize;
    auto pkt      = new sendbuf_t(sz + 6);
    auto body_ptr = init_sendbuf(pkt, eFRAG, sz, flags);
    htobe64buf(body_ptr, msgid);
    body_ptr[8] = frag.first;
    memcpy(body_ptr + 9, frag.second.data(), fragsize);
    queue.Put(pkt);
  }
  lastRetransmit = llarp_time_now_ms();
}

bool
transit_message::reassemble(std::vector< byte_t > &buffer)
{
  auto total = msginfo.totalsize();
  buffer.resize(total);
  auto fragsz = msginfo.fragsize();
  auto ptr    = &buffer[0];
  for(byte_t idx = 0; idx < msginfo.numfrags(); ++idx)
  {
    if(!status.test(idx))
      return false;
    memcpy(ptr, frags[idx].data(), fragsz);
    ptr += fragsz;
  }
  memcpy(ptr, lastfrag.data(), lastfrag.size());
  return true;
}

void
transit_message::put_message(llarp_buffer_t buf, const byte_t *hash,
                             uint64_t id, uint16_t mtu)
{
  status.reset();
  uint8_t fragid    = 0;
  uint16_t fragsize = mtu;
  size_t left       = buf.sz;
  while(left > fragsize)
  {
    auto &frag = frags[fragid];
    frag.resize(fragsize);
    memcpy(frag.data(), buf.cur, fragsize);
    buf.cur += fragsize;
    fragid++;
    left -= fragsize;
  }
  uint16_t lastfrag = buf.sz - (buf.cur - buf.base);
  // set info for xmit
  msginfo.set_info(hash, id, fragsize, lastfrag, fragid);
  put_lastfrag(buf.cur, lastfrag);
}

void
transit_message::put_lastfrag(byte_t *buf, size_t sz)
{
  lastfrag.resize(sz);
  memcpy(lastfrag.data(), buf, sz);
}

bool
transit_message::put_frag(byte_t fragno, byte_t *buf)
{
  auto itr = frags.find(fragno);
  if(itr == frags.end())
    return false;
  memcpy(itr->second.data(), buf, msginfo.fragsize());
  status.set(fragno);
  return true;
}

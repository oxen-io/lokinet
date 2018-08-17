#pragma once

#include <llarp/codel.hpp>
#include <llarp/crypto.hpp>
#include "frame_header.hpp"
#include "inbound_message.hpp"
#include "llarp/logger.hpp"
#include "llarp/time.h"
#include "llarp/types.h"
#include "sendbuf.hpp"
#include "sendqueue.hpp"
#include "transit_message.hpp"

#include <queue>
#include <unordered_map>

enum msgtype
{
  eALIV = 0x00,
  eXMIT = 0x01,
  eACKS = 0x02,
  eFRAG = 0x03
};

static inline byte_t *
init_sendbuf(sendbuf_t *buf, msgtype t, uint16_t sz, uint8_t flags)
{
  frame_header hdr(buf->data());
  hdr.version() = 0;
  hdr.msgtype() = t;
  hdr.setsize(sz);
  buf->data()[4] = 0;
  buf->data()[5] = flags;
  return hdr.data();
}

struct llarp_router;
struct llarp_link_session;

struct frame_state
{
  byte_t rxflags         = 0;
  byte_t txflags         = 0;
  uint64_t rxids         = 0;
  uint64_t txids         = 0;
  llarp_time_t lastEvent = 0;
  std::unordered_map< uint64_t, llarp::ShortHash > rxIDs;
  std::unordered_map< llarp::ShortHash, transit_message *,
                      llarp::ShortHash::Hash >
      rx;
  std::unordered_map< uint64_t, transit_message * > tx;

  // typedef std::queue< sendbuf_t * > sendqueue_t;

  typedef llarp::util::CoDelQueue<
      InboundMessage, InboundMessage::GetTime, InboundMessage::PutTime,
      InboundMessage::OrderCompare, llarp::util::DummyMutex,
      llarp::util::DummyLock >
      recvqueue_t;

  llarp_link_session *parent = nullptr;

  sendqueue_t sendqueue;
  recvqueue_t recvqueue;
  uint64_t nextMsgID = 0;

  frame_state(llarp_link_session *session)
      : parent(session)
      , sendqueue("iwp_outbound_message")
      , recvqueue("iwp_inbound_message")
  {
  }

  /// return true if both sides have the same state flags
  bool
  flags_agree(byte_t flags) const;

  bool
  process_inbound_queue();

  llarp_router *
  Router();

  void
  clear();

  bool
  inbound_frame_complete(uint64_t id);

  void
  push_ackfor(uint64_t id, uint32_t bitmask);

  bool
  got_xmit(frame_header hdr, size_t sz);

  void
  alive();

  bool
  got_frag(frame_header hdr, size_t sz);

  bool
  got_acks(frame_header hdr, size_t sz);

  // queue new outbound message
  void
  queue_tx(uint64_t id, transit_message *msg);

  void
  retransmit(llarp_time_t now);

  // get next frame to encrypt and transmit
  bool
  next_frame(llarp_buffer_t *buf);

  void
  pop_next_frame();

  bool
  process(byte_t *buf, size_t sz);
};

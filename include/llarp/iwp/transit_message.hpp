#pragma once

#include "llarp/types.h"
#include "sendbuf.hpp"
#include "sendqueue.hpp"
#include "xmit.hpp"

#include <bitset>
#include <unordered_map>
#include <vector>

struct transit_message
{
  xmit msginfo;
  std::bitset< 32 > status = {};

  typedef std::vector< byte_t > fragment_t;

  std::unordered_map< byte_t, fragment_t > frags;
  fragment_t lastfrag;
  llarp_time_t lastAck        = 0;
  llarp_time_t lastRetransmit = 0;
  llarp_time_t started;

  void
  clear();

  // calculate acked bitmask
  uint32_t
  get_bitmask() const;

  // outbound
  transit_message(llarp_buffer_t buf, const byte_t *hash, uint64_t id,
                  uint16_t mtu = 1024);

  // inbound
  transit_message(const xmit &x);

  /// ack packets based off a bitmask
  void
  ack(uint32_t bitmask);

  bool
  should_send_ack(llarp_time_t now) const;

  bool
  should_resend_frags(llarp_time_t now) const;

  bool
  should_resend_xmit(llarp_time_t now) const;
  bool
  completed() const;

  // template < typename T >
  void
  generate_xmit(sendqueue_t &queue, byte_t flags = 0);

  // template < typename T >
  void
  retransmit_frags(sendqueue_t &queue, byte_t flags = 0);

  bool
  reassemble(std::vector< byte_t > &buffer) const;

  void
  put_message(llarp_buffer_t buf, const byte_t *hash, uint64_t id,
              uint16_t mtu = 1024);

  void
  put_lastfrag(byte_t *buf, size_t sz);

  bool
  put_frag(byte_t fragno, byte_t *buf);
};

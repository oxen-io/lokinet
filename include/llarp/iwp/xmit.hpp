#pragma once

#include "llarp/buffer.h"

struct xmit
{
  byte_t buffer[48];

  xmit() = default;

  xmit(byte_t *ptr);

  xmit(const xmit &other);

  void
  set_info(const byte_t *hash, uint64_t id, uint16_t fragsz, uint16_t lastsz,
           uint8_t numfrags, uint8_t flags = 0x01);

  const byte_t *
  hash() const;

  uint64_t
  msgid() const;

  // size of each full fragment
  uint16_t
  fragsize() const;

  // number of full fragments
  uint8_t
  numfrags() const;

  // size of the entire message
  size_t
  totalsize() const;

  // size of the last fragment
  uint16_t
  lastfrag() const;

  uint8_t
  flags();
};

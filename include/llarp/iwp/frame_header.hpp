#pragma once

#include "llarp/buffer.h"

enum header_flag
{
  eSessionInvalidated = (1 << 0),
  eHighPacketDrop     = (1 << 1),
  eHighMTUDetected    = (1 << 2),
  eProtoUpgrade       = (1 << 3)
};

struct frame_header
{
  byte_t *ptr;

  frame_header(byte_t *buf);

  byte_t *
  data();

  uint8_t &
  version();

  uint8_t &
  msgtype();

  uint16_t
  size() const;

  void
  setsize(uint16_t sz);

  uint8_t &
  flags();

  void
  setflag(header_flag f);
};

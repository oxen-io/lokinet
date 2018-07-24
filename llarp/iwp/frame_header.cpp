#include "llarp/iwp/frame_header.hpp"
#include "llarp/endian.h"

frame_header::frame_header(byte_t *buf) : ptr(buf)
{
}

byte_t *
frame_header::data()
{
  return ptr + 6;
}

uint8_t &
frame_header::version()
{
  return ptr[0];
}

uint8_t &
frame_header::msgtype()
{
  return ptr[1];
}

uint16_t
frame_header::size() const
{
  return bufbe16toh(ptr + 2);
}

void
frame_header::setsize(uint16_t sz)
{
  htobe16buf(ptr + 2, sz);
}

uint8_t &
frame_header::flags()
{
  return ptr[5];
}

void
frame_header::setflag(header_flag f)
{
  ptr[5] |= f;
}

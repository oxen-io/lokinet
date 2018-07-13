#include "llarp/iwp/frame_header.hpp"

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
  uint16_t sz;
  memcpy(&sz, ptr + 2, 2);
  return sz;
}

void
frame_header::setsize(uint16_t sz)
{
  memcpy(ptr + 2, &sz, 2);
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

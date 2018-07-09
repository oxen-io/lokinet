#include "llarp/iwp/xmit.hpp"

xmit::xmit(byte_t *ptr)
{
  memcpy(buffer, ptr, sizeof(buffer));
}

xmit::xmit(const xmit &other)
{
  memcpy(buffer, other.buffer, sizeof(buffer));
}

void
xmit::set_info(const byte_t *hash, uint64_t id, uint16_t fragsz,
               uint16_t lastsz, uint8_t numfrags, uint8_t flags)
{
  // big endian assumed
  // TODO: implement little endian
  memcpy(buffer, hash, 32);
  memcpy(buffer + 32, &id, 8);
  memcpy(buffer + 40, &fragsz, 2);
  memcpy(buffer + 42, &lastsz, 2);
  buffer[44] = 0;
  buffer[45] = 0;
  buffer[46] = numfrags;
  buffer[47] = flags;
}

const byte_t *
xmit::hash() const
{
  return &buffer[0];
}

uint64_t
xmit::msgid() const
{
  // big endian assumed
  // TODO: implement little endian
  const byte_t *start   = buffer + 32;
  const uint64_t *msgid = (const uint64_t *)start;
  return *msgid;
}

// size of each full fragment
uint16_t
xmit::fragsize() const
{
  // big endian assumed
  // TODO: implement little endian
  const byte_t *start    = buffer + 40;
  const uint16_t *fragsz = (uint16_t *)start;
  return *fragsz;
}

// number of full fragments
uint8_t
xmit::numfrags() const
{
  return buffer[46];
}

// size of the entire message
size_t
xmit::totalsize() const
{
  return (fragsize() * numfrags()) + lastfrag();
}

// size of the last fragment
uint16_t
xmit::lastfrag() const
{
  // big endian assumed
  // TODO: implement little endian
  const byte_t *start    = buffer + 42;
  const uint16_t *lastsz = (uint16_t *)start;
  return *lastsz;
}

uint8_t
xmit::flags()
{
  return buffer[47];
}

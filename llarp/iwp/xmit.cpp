#include "llarp/iwp/xmit.hpp"
#include "llarp/endian.h"
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
  memcpy(buffer, hash, 32);
  // memcpy(buffer + 32, &id, 8);
  htobe64buf(buffer + 32, id);
  // memcpy(buffer + 40, &fragsz, 2);
  htobe16buf(buffer + 40, fragsz);
  // memcpy(buffer + 42, &lastsz, 2);
  htobe16buf(buffer + 42, lastsz);
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
  return bufbe64toh(buffer + 32);
}

// size of each full fragment
uint16_t
xmit::fragsize() const
{
  return bufbe16toh(buffer + 40);
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
  return bufbe16toh(buffer + 42);
}

uint8_t
xmit::flags()
{
  return buffer[47];
}

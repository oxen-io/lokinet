#include "buffer.hpp"

#include <oxenc/endian.h>

#include <cstdarg>
#include <cstdio>

bool
llarp_buffer_t::writef(const char* fmt, ...)
{
  int written;
  size_t toWrite = size_left();
  va_list args;
  va_start(args, fmt);
  written = vsnprintf(reinterpret_cast<char*>(cur), toWrite, fmt, args);
  va_end(args);
  if (written <= 0)
    return false;
  if (toWrite < static_cast<size_t>(written))
    return false;
  cur += written;
  return true;
}

namespace
{
  template <typename UInt>
  bool
  put(llarp_buffer_t& buf, UInt i)
  {
    if (buf.size_left() < sizeof(UInt))
      return false;
    oxenc::write_host_as_big(i, buf.cur);
    buf.cur += sizeof(UInt);
    return true;
  }

  template <typename UInt>
  bool
  read(llarp_buffer_t& buf, UInt& i)
  {
    if (buf.size_left() < sizeof(UInt))
      return false;
    i = oxenc::load_big_to_host<UInt>(buf.cur);
    buf.cur += sizeof(UInt);
    return true;
  }

}  // namespace

bool
llarp_buffer_t::put_uint16(uint16_t i)
{
  return put(*this, i);
}

bool
llarp_buffer_t::put_uint64(uint64_t i)
{
  return put(*this, i);
}

bool
llarp_buffer_t::put_uint32(uint32_t i)
{
  return put(*this, i);
}

bool
llarp_buffer_t::read_uint16(uint16_t& i)
{
  return read(*this, i);
}

bool
llarp_buffer_t::read_uint32(uint32_t& i)
{
  return read(*this, i);
}

bool
llarp_buffer_t::read_uint64(uint64_t& i)
{
  return read(*this, i);
}

size_t
llarp_buffer_t::read_until(char c_delim, byte_t* result, size_t resultsize)
{
  const auto delim = static_cast<byte_t>(c_delim);
  size_t read = 0;

  // do the bound check first, to avoid over running
  while ((cur != base + sz) && *cur != delim && resultsize)
  {
    *result = *cur;
    cur++;
    result++;
    resultsize--;
    read++;
  }

  if (size_left())
    return read;

  return 0;
}

std::vector<byte_t>
llarp_buffer_t::copy() const
{
  std::vector<byte_t> copy;
  copy.resize(sz);
  std::copy_n(base, sz, copy.data());

  return copy;
}

namespace llarp
{
  std::vector<byte_t>
  OwnedBuffer::copy() const
  {
    std::vector<byte_t> ret;
    ret.resize(sz);
    const auto* ptr = buf.get();
    std::copy(ptr, ptr + sz, ret.data());
    return ret;
  }

  OwnedBuffer
  OwnedBuffer::copy_from(const llarp_buffer_t& b)
  {
    auto buf = std::make_unique<byte_t[]>(b.sz);
    std::copy(b.begin(), b.end(), buf.get());
    return {std::move(buf), b.sz};
  }

  OwnedBuffer
  OwnedBuffer::copy_used(const llarp_buffer_t& b)
  {
    const size_t sz = b.cur - b.base;
    auto buf = std::make_unique<byte_t[]>(sz);
    std::copy(b.base, b.cur, buf.get());
    return {std::move(buf), sz};
  }

}  // namespace llarp

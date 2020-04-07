#include <util/buffer.hpp>
#include <util/endian.hpp>

#include <cstdarg>
#include <cstdio>

size_t
llarp_buffer_t::size_left() const
{
  size_t diff = cur - base;
  if (diff > sz)
  {
    return 0;
  }

  return sz - diff;
}

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

bool
llarp_buffer_t::put_uint16(uint16_t i)
{
  if (size_left() < sizeof(uint16_t))
    return false;
  htobe16buf(cur, i);
  cur += sizeof(uint16_t);
  return true;
}

bool
llarp_buffer_t::put_uint64(uint64_t i)
{
  if (size_left() < sizeof(uint64_t))
    return false;
  htobe64buf(cur, i);
  cur += sizeof(uint64_t);
  return true;
}

bool
llarp_buffer_t::put_uint32(uint32_t i)
{
  if (size_left() < sizeof(uint32_t))
    return false;
  htobe32buf(cur, i);
  cur += sizeof(uint32_t);
  return true;
}

bool
llarp_buffer_t::read_uint16(uint16_t& i)
{
  if (size_left() < sizeof(uint16_t))
    return false;
  i = bufbe16toh(cur);
  cur += sizeof(uint16_t);
  return true;
}

bool
llarp_buffer_t::read_uint32(uint32_t& i)
{
  if (size_left() < sizeof(uint32_t))
    return false;
  i = bufbe32toh(cur);
  cur += sizeof(uint32_t);
  return true;
}

bool
llarp_buffer_t::read_uint64(uint64_t& i)
{
  if (size_left() < sizeof(uint64_t))
    return false;
  i = bufbe64toh(cur);
  cur += sizeof(uint64_t);
  return true;
}

size_t
llarp_buffer_t::read_until(char delim, byte_t* result, size_t resultsize)
{
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

bool
operator==(const llarp_buffer_t& buff, const char* str)
{
  ManagedBuffer copy{buff};
  while (*str && copy.underlying.cur != (copy.underlying.base + copy.underlying.sz))
  {
    if (*copy.underlying.cur != *str)
      return false;
    copy.underlying.cur++;
    str++;
  }
  return *str == 0;
}

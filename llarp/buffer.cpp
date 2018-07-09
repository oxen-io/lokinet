#include <llarp/buffer.h>
#include <stdarg.h>
#include <stdio.h>

size_t
llarp_buffer_size_left(llarp_buffer_t buff)
{
  size_t diff = buff.cur - buff.base;
  if(diff > buff.sz)
    return 0;
  else
    return buff.sz - diff;
}

bool
llarp_buffer_writef(llarp_buffer_t* buff, const char* fmt, ...)
{
  int written;
  ssize_t sz = llarp_buffer_size_left(*buff);
  va_list args;
  va_start(args, fmt);
  written = vsnprintf((char*)buff->cur, sz, fmt, args);
  va_end(args);
  if(written <= 0)
    return false;
  if(sz < written)
    return false;
  buff->cur += written;
  return true;
}

bool
llarp_buffer_write(llarp_buffer_t* buff, const void* data, size_t sz)
{
  size_t left = llarp_buffer_size_left(*buff);
  if(left >= sz)
  {
    memcpy(buff->cur, data, sz);
    buff->cur += sz;
    return true;
  }
  return false;
}

size_t
llarp_buffer_read_until(llarp_buffer_t* buff, char delim, byte_t* result,
                        size_t resultsize)
{
  size_t read = 0;

  while(*buff->cur != delim && resultsize
        && (buff->cur != buff->base + buff->sz))
  {
    *result = *buff->cur;
    buff->cur++;
    result++;
    resultsize--;
    read++;
  }

  if(llarp_buffer_size_left(*buff))
    return read;
  else
    return 0;
}

bool
llarp_buffer_eq(llarp_buffer_t buf, const char* str)
{
  while(*str && buf.cur != (buf.base + buf.sz))
  {
    if(*buf.cur != *str)
      return false;
    buf.cur++;
    str++;
  }
  return *str == 0;
}

#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <buffer.h>

namespace llarp
{
  template < typename T >
  llarp_buffer_t
  StackBuffer(T& stack)
  {
    llarp_buffer_t buff;
    buff.base = &stack[0];
    buff.cur  = buff.base;
    buff.sz   = sizeof(stack);
    return buff;
  }

  /** initialize llarp_buffer_t from raw memory */
  template < typename T >
  llarp_buffer_t
  InitBuffer(T buf, size_t sz)
  {
    byte_t* ptr = (byte_t*)buf;
    llarp_buffer_t ret;
    ret.cur  = ptr;
    ret.base = ptr;
    ret.sz   = sz;
    return ret;
  }

  /** initialize llarp_buffer_t from container */
  template < typename T >
  llarp_buffer_t
  Buffer(T& t)
  {
    llarp_buffer_t buff;
    buff.base = &t[0];
    buff.cur  = buff.base;
    buff.sz   = t.size();
    return buff;
  }

  template < typename T >
  llarp_buffer_t
  ConstBuffer(const T& t)
  {
    llarp_buffer_t buff;
    buff.base = (byte_t*)&t[0];
    buff.cur  = buff.base;
    buff.sz   = t.size();
    return buff;
  }

}  // namespace llarp

#endif

#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <llarp/buffer.h>

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
}  // namespace llarp

#endif

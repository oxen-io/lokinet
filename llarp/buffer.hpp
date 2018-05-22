#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <llarp/buffer.h>

namespace llarp
{
  /** initialize llarp_buffer_t from stack allocated buffer */
  template < typename T >
  void
  StackBuffer(llarp_buffer_t& buff, T& stack)
  {
    buff.base = stack;
    buff.cur  = buff.base;
    buff.sz   = sizeof(stack);
  }

  template < typename T >
  llarp_buffer_t
  StackBuffer(T& stack)
  {
    llarp_buffer_t buff;
    buff.base = stack;
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
}

#endif

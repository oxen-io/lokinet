#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <llarp/buffer.h>

namespace llarp
{
  /** initialize llarp_buffer_t from stack allocated buffer */
  template<typename T>
  llarp_buffer_t StackBuffer(T & stack)
  {
    llarp_buffer_t buff;
    buff.base = (char*) stack;
    buff.cur = (char*) stack;
    buff.sz = sizeof(stack);
    return buff;
  }
}



#endif

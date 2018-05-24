#ifndef LLARP_FRAME_HANDLER_HPP
#define LLARP_FRAME_HANDLER_HPP
#include <llarp/mem.h>
#include <vector>

namespace llarp
{
  struct FrameHandler
  {
    bool
    Process(const std::vector< byte_t >& buffer);
  };
}

#endif

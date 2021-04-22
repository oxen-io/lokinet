#include "ev.hpp"
#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>

#include <cstddef>
#include <cstring>
#include <string_view>

// We libuv now
#include "ev_libuv.hpp"

namespace llarp
{
  EventLoop_ptr
  EventLoop::create(size_t queueLength)
  {
    return std::make_shared<llarp::uv::Loop>(queueLength);
  }
}  // namespace llarp

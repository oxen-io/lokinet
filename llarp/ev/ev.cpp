#include "ev.hpp"
#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>

#include <cstddef>
#include <cstring>
#include <string_view>

#include "libuv.hpp"
#include <llarp/net/net.hpp>

namespace llarp
{
  EventLoop_ptr
  EventLoop::create(size_t queueLength)
  {
    return std::make_shared<llarp::uv::Loop>(queueLength);
  }

  const net::Platform*
  EventLoop::Net_ptr() const
  {
    return net::Platform::Default_ptr();
  }

}  // namespace llarp

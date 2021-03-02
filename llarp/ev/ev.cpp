#include <ev/ev.hpp>
#include <util/mem.hpp>
#include <util/str.hpp>
#include <util/thread/logic.hpp>

#include <cstddef>
#include <cstring>
#include <string_view>

// We libuv now
#include <ev/ev_libuv.hpp>

namespace llarp
{
  EventLoop_ptr
  EventLoop::create(size_t queueLength)
  {
    return std::make_shared<llarp::uv::Loop>(queueLength);
  }

  void
  EventLoop::run(Logic& logic)
  {
    run_loop();
    logic.clear_event_loop();
    stopped();
  }

}  // namespace llarp

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
  EventLoop::create(int threads)
  {
#ifndef _WIN32
#ifdef __linux__
    if (threads <= 0)
      threads = std::thread::hardware_concurrency();

    auto threads_str = std::to_string(threads);
    ::setenv("UV_THREADPOOL_SIZE", threads_str.c_str(), 1);
#endif
#endif
    (void)threads;
    return std::make_shared<llarp::uv::Loop>(event_loop_queue_size);
  }
}  // namespace llarp

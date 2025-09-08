#pragma once
#include <oxen/quic/loop.hpp>

namespace llarp::ev
{
    /** FDPoller
     *
     * This poller watches a file descriptor for readability, triggering when the fd becomes
     * readable.
     */
    class FDPoller
    {
      public:
        // No move/copy/etc
        FDPoller() = delete;
        FDPoller(const FDPoller&) = delete;
        FDPoller(FDPoller&&) = delete;
        FDPoller& operator=(const FDPoller&) = delete;
        FDPoller& operator=(FDPoller&&) = delete;

        // Starts a file decriptor poller that watches the given file descriptor via the given event
        // loop.  Caller must ensure that the loop remains valid for the lifetime of the constructed
        // FDPoller.
        FDPoller(oxen::quic::Loop& loop, int fd, std::function<void()> on_readable);

      private:
        int _fd;
        oxen::quic::event_ptr _ev;
        std::function<void()> _on_readable;
    };

}  // namespace llarp::ev

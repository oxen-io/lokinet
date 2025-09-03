#include "fd_poller.hpp"
#include <llarp/util/logging.hpp>
#include <event2/event.h>

namespace llarp::ev {

    static auto logcat = log::Cat("ev.fd");

    FDPoller::FDPoller(oxen::quic::Loop& loop, int fd, std::function<void()> on_readable)
        : _fd{fd}, _on_readable{std::move(on_readable)}
    {
        if (!_on_readable)
            throw std::invalid_argument{"FDPoller requires non-null on_readable function"};
        _ev.reset(event_new(
            loop.get_event_base(),
            _fd,
            EV_READ | EV_PERSIST,
            [](evutil_socket_t, short, void* s) {
                assert(s);
                auto* self = static_cast<FDPoller*>(s);
                try
                {
                    self->_on_readable();
                }
                catch (const std::exception& e)
                {
                    log::error(logcat, "FDPoller callback raised uncaught exception: {}", e.what());
                }
            },
            this));
        if (!_ev || 0 != event_add(_ev.get(), nullptr))
            throw std::invalid_argument{"Failed to create libevent event!"};

        log::debug(logcat, "FD poller watching fd {}", _fd);
    }
}

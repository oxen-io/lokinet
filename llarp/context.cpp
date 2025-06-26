#include <llarp.hpp>
#include <llarp/config/config.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/service_manager.hpp>

#include <oxen/quic/loop.hpp>

#include <csignal>
#include <memory>
#include <stdexcept>

#if (__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
    static auto logcat = log::Cat("context");

    // Defaulted here because the header doesn't have visibility of the unique_ptr destructors.
    Context::~Context() = default;

    bool Context::is_up() const { return router && router->is_running(); }

    bool Context::looks_alive() const { return router && router->looks_alive(); }

    int Context::run(Config conf)
    {
        log::debug(logcat, "Initializing event loop...");

        loop = std::make_shared<quic::Loop>();
        log::debug(logcat, "Event loop initialized!");

        auto done_promise = std::promise<void>();
        auto done_future = done_promise.get_future();

        log::debug(logcat, "Initializing platform code...");
        auto plat = vpn::MakeNativePlatform(this);
        if (plat == nullptr)
            throw std::runtime_error{"This platform is not currently supported!"};

        log::debug(logcat, "Starting main router...");
        try
        {
            router = std::make_unique<Router>(std::move(conf), loop, std::move(plat), std::move(done_promise));
        }
        catch (const std::exception& e)
        {
            log::error(logcat, "Failed to initialize router: {}", e.what());
            return 2;
        }

        done_future.wait();
        router.reset();

        if (std::lock_guard lock{close_waiter_mut}; close_waiter)
        {
            close_waiter->set_value();
            close_waiter.reset();
        }

        return 0;
    }

    void Context::close_async()
    {
        {
            std::lock_guard lock{close_waiter_mut};
            if (close_waiter)
                return;  // already closing
            close_waiter.emplace();
        }

        loop->call([this] { handle_signal(SIGTERM); });
    }

    bool Context::is_stopping() const
    {
        std::lock_guard lock{close_waiter_mut};
        return close_waiter.has_value();
    }

    void Context::wait()
    {
        if (close_waiter)
        {
            close_waiter->get_future().wait();
            close_waiter.reset();
        }
    }

    void Context::handle_signal(int sig)
    {
        assert(loop->inside());
        if (router && (sig == SIGINT || sig == SIGTERM))
        {
            log::warning(logcat, "Received signal SIG{}; stopping router...", sig == SIGINT ? "INT" : "TERM");
            router->stop();
        }
    }

    Context::Context()
    {
        // service_manager is a global and context isnt
        llarp::sys::service_manager->give_context(this);
    }

}  // namespace llarp

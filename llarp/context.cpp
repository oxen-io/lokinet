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

    bool Context::is_waiting() const { return router && !router->is_running(); }

    bool Context::looks_alive() const { return router && router->looks_alive(); }

    void Context::start(Config conf, std::shared_ptr<oxen::quic::Loop> loop)
    {
        if (router)
        {
            log::error(logcat, "Context::start called but Lokinet is already running");
            throw std::logic_error{"Lokinet is already started"};
        }
        if (loop)
            log::debug(logcat, "Re-using existing loop");
        else
        {
            log::debug(logcat, "Initializing event loop...");

            loop = std::make_shared<quic::Loop>();
            log::debug(logcat, "Event loop initialized!");
        }

        std::promise<void> done_promise;
        lifetime_waiter = done_promise.get_future();

        log::debug(logcat, "Initializing platform code...");
        auto plat = vpn::MakeNativePlatform(this);
        if (plat == nullptr)
            throw std::runtime_error{"This platform is not currently supported!"};

        log::debug(logcat, "Starting main router...");
        try
        {
            router =
                std::make_unique<Router>(std::move(conf), std::move(loop), std::move(plat), std::move(done_promise));
        }
        catch (const std::exception& e)
        {
            log::error(logcat, "Failed to initialize router: {}", e.what());
            throw;
        }
    }

    void Context::wait()
    {
        if (!router)
            return;
        lifetime_waiter.get();
        router.reset();
    }

    void Context::stop()
    {
        if (!router)
            return;
        router->stop();
    }

    bool Context::is_stopping() const { return router && router->is_stopping(); }

    void Context::signal(int sig)
    {
        if (router && (sig == SIGINT || sig == SIGTERM || sig == SIGKILL))
        {
            log::warning(
                logcat,
                "Received signal SIG{}; stopping router...",
                sig == SIGINT        ? "INT"
                    : sig == SIGTERM ? "TERM"
                                     : "KILL");
            stop();
        }
    }

    Context::Context()
    {
        // service_manager is a global and context isnt
        llarp::sys::service_manager->give_context(this);
    }

}  // namespace llarp

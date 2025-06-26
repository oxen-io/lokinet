#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <optional>

namespace oxen::quic
{
    class Loop;
}

namespace llarp
{
    namespace vpn
    {
        class Platform;
    }

    struct Config;
    class Router;

    struct Context
    {
        std::unique_ptr<Router> router;
        std::shared_ptr<oxen::quic::Loop> loop;

        Context();
        ~Context();

        // Runs Lokinet.  Does not return until Lokinet stops, i.e. by something else calling
        // close() or close_async().
        int run(Config conf);

        void handle_signal(int sig);

        bool is_up() const;

        bool looks_alive() const;

        bool is_stopping() const;

        /// close async
        void close_async();

        /// wait until closed and done (call close_async first, then this).
        void wait();

        /// close async + wait
        void close()
        {
            close_async();
            wait();
        }

        int androidFD = -1;

      private:
        mutable std::mutex close_waiter_mut;
        std::optional<std::promise<void>> close_waiter;
    };
}  // namespace llarp

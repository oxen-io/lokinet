#pragma once

#include <llarp/address/types.hpp>

#include <optional>
#include <unordered_map>

namespace llarp
{
    class Router;

    template <typename IP>
    concept IP46 = std::same_as<IP, ipv4> || std::same_as<IP, ipv6>;

    class RoutePoker
    {
      public:
        RoutePoker(Router& r);
        RoutePoker(const RoutePoker&) = delete;
        RoutePoker(RoutePoker&&) = delete;
        RoutePoker& operator=(const RoutePoker&) = delete;
        RoutePoker& operator=(RoutePoker&&) = delete;

        template <IP46 IP>
        void add_route(const IP& ip);

        template <IP46 IP>
        void delete_route(const IP& ip)
        {
            auto& pr = poked_routes<IP>();
            if (auto it = pr.find(ip); it != pr.end())
                delete_route(it);
        }

        void start();

        ~RoutePoker();

        /// explicitly put routes up
        void put_up();

        /// explicitly put routes down
        void put_down();

        /// set dns resolver
        /// pass in if we are using exit node mode right now  as a bool
        // void set_dns_mode(bool using_exit_mode) const;

        bool enabled() const { return _enabled; }

      private:
        void update();

        void delete_all_routes();

        void disable_all_routes();

        void refresh_all_routes();

        template <IP46 IP>
        void enable_route(const IP& ip, const IP& gateway);

        template <IP46 IP>
        void disable_route(const IP& ip, const IP& gateway);

        std::unordered_map<ipv4, ipv4> poked_routes4;
        std::unordered_map<ipv6, ipv6> poked_routes6;
        std::optional<ipv4> current_gateway4;
        std::optional<ipv6> current_gateway6;

        template <IP46 IP>
        std::unordered_map<IP, IP>& poked_routes()
        {
            if constexpr (std::same_as<IP, ipv4>)
                return poked_routes4;
            else
                return poked_routes6;
        }
        template <IP46 IP>
        std::optional<IP>& current_gateway()
        {
            if constexpr (std::same_as<IP, ipv4>)
                return current_gateway4;
            else
                return current_gateway6;
        }

        template <IP46 IP>
        using poked_iterator = std::unordered_map<IP, IP>::iterator;

        template <typename It>
            requires std::same_as<It, poked_iterator<ipv4>> || std::same_as<It, poked_iterator<ipv6>>
        auto delete_route(It it)
        {
            disable_route(it->first, it->second);
            using IP = std::conditional_t<std::same_as<It, poked_iterator<ipv4>>, ipv4, ipv6>;
            return poked_routes<IP>().erase(it);
        }

        Router& _router;
        bool _up{false};
        bool _enabled{false};
    };

    extern template void RoutePoker::add_route(const ipv4&);
    extern template void RoutePoker::add_route(const ipv6&);
    extern template void RoutePoker::enable_route(const ipv4& ip, const ipv4& gateway);
    extern template void RoutePoker::enable_route(const ipv6& ip, const ipv6& gateway);
    extern template void RoutePoker::disable_route(const ipv4& ip, const ipv4& gateway);
    extern template void RoutePoker::disable_route(const ipv6& ip, const ipv6& gateway);

}  // namespace llarp

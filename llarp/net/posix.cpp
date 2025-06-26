#include "net_if.hpp"
#include "platform.hpp"

#include <llarp/address/ip_range.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/logging.hpp>

#include <stdexcept>

#ifdef ANDROID
#include <llarp/android/ifaddrs.h>
#else
#include <ifaddrs.h>
#endif

#include <oxen/quic/address.hpp>

namespace llarp::net
{
    static auto logcat = log::Cat("posix");

    namespace
    {
        struct ifaddrs_deleter
        {
            void operator()(ifaddrs* ia) const noexcept { freeifaddrs(ia); }
        };
        using ifaddrs_ptr = std::unique_ptr<ifaddrs, ifaddrs_deleter>;
        ifaddrs_ptr getifaddrs()
        {
            if (ifaddrs * ia; 0 == ::getifaddrs(&ia))
                return ifaddrs_ptr{ia};
            throw std::runtime_error{"getifaddrs(): {}"_format(strerror(errno))};
        }
    }  // namespace

    class Platform_Impl : public Platform
    {
        // Iterates through interfaces.  If the Visitor returns a bool then a true return means to
        // break the iteration loop.
        template <std::invocable<const ifaddrs&> Visitor>
        void for_each_interface(Visitor&& visit) const
        {
            auto addrs = getifaddrs();
            for (auto next = addrs.get(); next; next = next->ifa_next)
            {
                const auto& n = *next;
                if constexpr (std::same_as<bool, decltype(visit(n))>)
                {
                    if (visit(n))
                        break;
                }
                else
                    visit(n);
            }
        }

      public:
        std::string loopback_interface_name() const override
        {
            std::string ifname;

            for_each_interface([&ifname](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET)
                {
                    const quic::Address addr{i.ifa_addr};
                    if (addr.is_loopback())
                    {
                        ifname = i.ifa_name;
                        return true;
                    }
                }
                return false;
            });

            if (ifname.empty())
                throw std::runtime_error{"we have no ipv4 loopback interface for some ungodly reason"};

            return ifname;
        }

        std::optional<quic::Address> get_best_public_address(bool ipv4, uint16_t port) const override
        {
            std::optional<quic::Address> found;

            for_each_interface([&found, family = ipv4 ? AF_INET : AF_INET6, port](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == family)
                {
                    quic::Address a{i.ifa_addr};
                    if (a.is_public_ip())
                    {
                        a.set_port(port);
                        found = std::move(a);
                        return true;
                    }
                }
                return false;
            });

            log::info(logcat, "get_best_public_address returned: {}", found);

            return found;
        }

        std::optional<ipv4_net> find_free_ipv4_net(uint8_t mask) const override
        {
            std::vector<ipv4_range> current_ranges;

            for_each_interface([&current_ranges](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET)
                {
                    ipv4 addr{reinterpret_cast<sockaddr_in*>(i.ifa_addr)->sin_addr};
                    auto mask = static_cast<uint8_t>(
                        std::popcount(reinterpret_cast<sockaddr_in*>(i.ifa_netmask)->sin_addr.s_addr));

                    log::debug(logcat, "Adding {}/{} to excluded search ranges", addr, mask);
                    current_ranges.emplace_back(addr, mask);
                }
            });

            return find_private_ipv4_net(std::move(current_ranges), mask);
        }

        std::optional<ipv6_net> find_free_ipv6_net(uint8_t mask) const override
        {
            std::vector<ipv6_range> current_ranges;

            for_each_interface([&current_ranges](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET6)
                {
                    ipv6 addr{reinterpret_cast<sockaddr_in6*>(i.ifa_addr)->sin6_addr};
                    uint8_t mask = 0;
                    for (auto c : reinterpret_cast<sockaddr_in6*>(i.ifa_netmask)->sin6_addr.s6_addr)
                        mask += std::popcount(c);
                    log::debug(logcat, "Adding {}/{} to excluded search ranges", addr, mask);
                    current_ranges.emplace_back(addr, mask);
                }
            });

            return find_private_ipv6_net(std::move(current_ranges), mask);
        }

        std::optional<int> get_interface_index(ipv4 ip) const override
        {
            std::optional<int> ret;

            for_each_interface([&ret, &ip](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET)
                {
                    auto if_ip = quic::Address{i.ifa_addr}.to_ipv4();
                    if (if_ip == ip)
                    {
                        ret = if_nametoindex(i.ifa_name);
                        return true;
                    }
                }
                return false;
            });

            return ret;
        }
        std::optional<int> get_interface_index(ipv6 ip) const override
        {
            std::optional<int> ret;

            for_each_interface([&ret, &ip](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET6)
                {
                    auto if_ip = quic::Address{i.ifa_addr}.to_ipv6();
                    if (if_ip == ip)
                    {
                        ret = if_nametoindex(i.ifa_name);
                        return true;
                    }
                }
                return false;
            });

            return ret;
        }

        std::string find_free_tun(std::string_view suggest) const override
        {
#ifdef __linux__
            if (!suggest.empty())
            {
                suggest = suggest.substr(0, IFNAMSIZ - 1);
                auto addrs = getifaddrs();
                bool found = false;
                for (auto next = addrs.get(); next; next = next->ifa_next)
                {
                    if (next->ifa_name && next->ifa_name == suggest)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return std::string{suggest};
            }
            // Let the kernel choose automatically:
            return "lokitun%d"s;
#else
            // On non-linux (e.g. FreeBSD) arbitrary tun device names can't be chosen, so we just
            // return "" to always auto-allocate.
            return ""s;
#endif
        }

        std::optional<ipv4> get_interface_ipv4(std::string_view ifname) const override
        {
            std::optional<ipv4> addr;

            for_each_interface([&addr, &ifname](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET and i.ifa_name == ifname)
                {
                    addr = quic::Address{i.ifa_addr}.to_ipv4();
                    return true;
                }
                return false;
            });

            return addr;
        }

        std::optional<ipv6> get_interface_ipv6(std::string_view ifname) const override
        {
            std::optional<ipv6> addr;

            for_each_interface([&addr, &ifname](const ifaddrs& i) {
                if (i.ifa_addr and i.ifa_addr->sa_family == AF_INET6 and i.ifa_name == ifname)
                {
                    addr = quic::Address{i.ifa_addr}.to_ipv6();
                    return true;
                }
                return false;
            });

            return addr;
        }

        bool has_interface_address(ipv4 ip) const override
        {
            bool found{false};
            for_each_interface([&found, &ip](const ifaddrs& i) {
                found = i.ifa_addr and i.ifa_addr->sa_family == AF_INET and ip == quic::Address{i.ifa_addr}.to_ipv4();
                return found;
            });
            return found;
        }
        bool has_interface_address(ipv6 ip) const override
        {
            bool found{false};
            for_each_interface([&found, &ip](const ifaddrs& i) {
                found = i.ifa_addr and i.ifa_addr->sa_family == AF_INET6 and ip == quic::Address{i.ifa_addr}.to_ipv6();
                return found;
            });
            return found;
        }
    };

    const Platform_Impl g_plat{};

    const Platform* Platform::Default_ptr() { return &g_plat; }
}  // namespace llarp::net

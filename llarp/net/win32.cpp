#include "net_if.hpp"
#include "platform.hpp"

#include <llarp/constants/platform.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>
#include <llarp/win32/exception.hpp>

#include <iphlpapi.h>

#include <cstdio>
#include <list>
#include <stdexcept>
#include <type_traits>

namespace llarp::net
{
    static auto logcat = log::Cat("win32.net");

    class Platform_Impl : public Platform
    {
        template <typename adapter_t>
        bool adapter_has_ip(adapter_t* a, ipaddr_t ip) const
        {
            for (auto* addr = a->FirstUnicastAddress; addr; addr = addr->Next)
            {
                quic::Address saddr{*addr->Address.lpSockaddr};

                log::debug(logcat, "'{}' has address '{}", a->AdapterName, saddr);
                if (saddr.getIP() == ip)
                    return true;
            }
            return false;
        }

        template <typename adapter_t>
        bool adapter_has_fam(adapter_t* a, int af) const
        {
            for (auto* addr = a->FirstUnicastAddress; addr; addr = addr->Next)
            {
                quic::Address saddr{*addr->Address.lpSockaddr};
                if (saddr.Family() == af)
                    return true;
            }
            return false;
        }

      public:
        std::optional<int> get_interface_index(ip ip) const override
        {
            std::optional<int> found;
            int af{AF_INET};

            if (std::holds_alternative<ipv6>(ip))
                af = AF_INET6;

            win32::iter_adapters(
                [&found, ip, this](auto* adapter) {
                    if (found)
                        return;

                    log::debug(
                        logcat,
                        "Visit adapter looking for '{}': '{} idx={}",
                        ip,
                        adapter->AdapterName,
                        adapter->IfIndex);

                    if (adapter_has_ip(adapter, ip))
                    {
                        found = adapter->IfIndex;
                    }
                },
                af);

            return found;
        }

        std::optional<quic::Address> get_interface_addr(std::string_view name, int af) const override
        {
            std::optional<quic::Address> found;

            win32::iter_adapters([name = std::string{name}, af, &found, this](auto* a) {
                if (found)
                    return;
                if (std::string{a->AdapterName} != name)
                    return;

                if (adapter_has_fam(a, af))
                    found = quic::Address{*a->FirstUnicastAddress->Address.lpSockaddr};
            });

            return found;
        }

        std::optional<quic::Address> all_interfaces(quic::Address fallback) const override
        {
            (void)fallback;
            // windows seems to not give a shit about source address
            return quic::Address{};
        }

        std::string find_free_tun() const override { return "lokitun0"; }

        std::optional<quic::Address> get_best_public_address(bool, uint16_t) const override
        {
            // TODO: implement me ?
            return std::nullopt;
        }

        std::optional<IPRange> find_free_range(bool ipv6_enabled) const override
        {
            std::list<IPRange> currentRanges;

            win32::iter_adapters([&currentRanges](auto* i) {
                for (auto* addr = i->FirstUnicastAddress; addr; addr = addr->Next)
                {
                    quic::Address saddr{*addr->Address.lpSockaddr};

                    bool is_ipv6 = addr->Address.lpSockaddr->sa_family == AF_INET6 ? true : false;

                    // TOFIX: wtf is this
                    uint8_t m = is_ipv6 ? reinterpret_cast<sockaddr_in6*>(*addr->Address.lpSockaddr)->sin6_addr.s6_addr
                                        : reinterpret_cast<sockaddr_in*>(*addr->Address.lpSockaddr)->sin_addr.s_addr;

                    currentRanges.emplace_back(std::move(saddr), m);
                }
            });

            return IPRange::find_private_range(currentRanges);
        }

        std::string loopback_interface_name() const override
        {
            // todo: implement me? does windows even have a loopback?
            return "";
        }

        bool has_interface_address(ip ip) const override { return get_interface_index(ip) != std::nullopt; }
    };

    const Platform_Impl g_plat{};

    const Platform* Platform::Default_ptr() { return &g_plat; }
}  // namespace llarp::net

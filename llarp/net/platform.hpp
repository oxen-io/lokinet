#pragma once
#include <llarp/address/types.hpp>

#include <oxen/log/format.hpp>
#include <oxen/quic/address.hpp>

#include <optional>

using namespace oxen::log::literals;

namespace llarp::net
{

    /// network platform (all methods virtual so it can be mocked by unit tests)
    class Platform
    {
      public:
        Platform() = default;
        virtual ~Platform() = default;
        Platform(const Platform&) = delete;
        Platform(Platform&&) = delete;

        /// get a pointer to our singleton instance used by full lokinet instances.
        /// embedded clients (and unit test mocks) will not call this
        static const Platform* Default_ptr();

        virtual bool has_interface_address(ipv4 ip) const = 0;
        virtual bool has_interface_address(ipv6 ip) const = 0;

        // Attempts to guess a good default public network address from the system's public IP
        // addresses; the returned Address (if set) will have its port set to the given value.
        virtual std::optional<quic::Address> get_best_public_address(bool ipv4, uint16_t port) const = 0;

        virtual std::optional<ipv4_net> find_free_ipv4_net(uint8_t mask = 16) const = 0;
        virtual std::optional<ipv6_net> find_free_ipv6_net(uint8_t /*mask*/ = 64) const { return std::nullopt; }

        // Attempts to find a usable tun device name.  This may return an empty string if naming
        // cannot be controlled, or a pattern (e.g. "lokitun%d") depending on the OS.  Note in
        // particular that that means the value returned here is merely suitable for creating the
        // tun, but not necessarily the final name.
        //
        // If suggest is given then we try that first before falling back to a generic name.  (Note
        // that the suggestion will be truncated at the OS name limit, e.g. 15 characters on linux).
        //
        // If possible (and no suggestion is given), we try to use lokitun0, lokitun1, etc.
        virtual std::string find_free_tun(std::string_view suggest = ""sv) const = 0;

        // Returns the IPv4 address of an interface, if it exists and has one, nullopt otherwise.
        virtual std::optional<ipv4> get_interface_ipv4(std::string_view ifname) const = 0;

        // Returns the IPv6 address of an interface, if it exists and has one, nullopt otherwise.
        virtual std::optional<ipv6> get_interface_ipv6(std::string_view ifname) const = 0;

        virtual std::optional<int> get_interface_index(ipv4 ip) const = 0;
        virtual std::optional<int> get_interface_index(ipv6 ip) const = 0;
    };

}  // namespace llarp::net

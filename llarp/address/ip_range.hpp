#pragma once

#include "types.hpp"

#include <oxen/quic/address.hpp>

namespace llarp
{
    // Takes an address with an embedded "/mask" at the end and splits it into address and mask.
    // Throws std::invalid_argument if not parseable as an ADDR/MASK value.  If default_mask is
    // given then the mask is optional, and the given default will be used if the mask is omitted
    // from the string.  (Otherwise the mask must be provided in the string).
    //
    // Throws on invalid input.
    ipv4_net parse_ipv4_net(std::string_view address, std::optional<uint8_t> default_mask = std::nullopt);
    ipv6_net parse_ipv6_net(std::string_view address, std::optional<uint8_t> default_mask = std::nullopt);

    // Exactly the same as above but returns a range (which differs from a net in that it ignores
    // the IP and always uses the base IP).
    ipv4_range parse_ipv4_range(std::string_view address, std::optional<uint8_t> default_mask = std::nullopt);
    ipv6_range parse_ipv6_range(std::string_view address, std::optional<uint8_t> default_mask = std::nullopt);

    // Extracts the ipv4 address from a quic::Address, applies the netmask, and returns in an
    // ipv{4,6}_{net,range}.  Throws if the Address contains the wrong type (ipv6 when ipv4 wanted or vice
    // versa).
    ipv4_net to_ipv4_net(const quic::Address& addr, uint8_t mask);
    ipv6_net to_ipv6_net(const quic::Address& addr, uint8_t mask);
    ipv4_net to_ipv4_range(const quic::Address& addr, uint8_t mask);
    ipv6_net to_ipv6_range(const quic::Address& addr, uint8_t mask);

    // binary encoding of an IP range, such as for exit policy ranges in a CC.
    std::string encode(const ipv4_range& r);
    std::string encode(const ipv6_range& r);
    ipv4_range decode_ipv4_range(std::string_view encoded);
    ipv6_range decode_ipv6_range(std::string_view decoded);
    std::variant<ipv4_range, ipv6_range> decode_ip_range(std::string_view encoded);

    /// IPRangeIterator - walks through an IP range
    template <bool IPv4 = true>
    struct IPRangeIterator
    {
        static constexpr bool is_ipv4 = IPv4;
        using ip_t = std::conditional_t<is_ipv4, ipv4, ipv6>;
        using ip_net_t = std::conditional_t<is_ipv4, ipv4_net, ipv6_net>;

      private:
        ip_t _curr, _base, _last;

      public:
        IPRangeIterator() = default;

        // Creates a range that starts at the current IP of range, increments up to the max
        // pre-broadcast address, and resets to the ".1" address of the range.
        explicit IPRangeIterator(const ip_net_t& net)
            : _curr{net.ip}, _base{_curr.to_base(net.mask)}, _last{net.max_ip()}
        {}

        // Returns the next ip address in the iterating range; returns nullopt if range is exhausted
        std::optional<ip_t> next_ip()
        {
            if (_curr == _last)
                return std::nullopt;
            if (auto next = _curr.next_ip())
                return _curr = *next;
            return std::nullopt;
        }

        // Resets the range to the base IP in the range so that next_ip() starts over from the
        // beginning of the range.
        void reset() { _curr = _base; }

        bool range_exhausted() const { return _curr == _last; }
    };
    IPRangeIterator(const ipv4_net&) -> IPRangeIterator<true>;
    IPRangeIterator(const ipv6_net&) -> IPRangeIterator<false>;

    using IPv4RangeIterator = IPRangeIterator<true>;
    using IPv6RangeIterator = IPRangeIterator<false>;

    // Finds a private IP /N range that does not overlap with any ranges in `excluding`.  Returns
    // the ipv{4,6}_net with ip set to the first usable address in the range (i.e. the ".1" or "::1"
    // for an IPv4 mask_size of 24 or smaller).  Returns nullopt if no suitable range can be found.
    std::optional<ipv4_net> find_private_ipv4_net(const std::vector<ipv4_range>& excluding, uint8_t mask_size);
    std::optional<ipv6_net> find_private_ipv6_net(const std::vector<ipv6_range>& excluding, uint8_t mask_size);

}  //  namespace llarp

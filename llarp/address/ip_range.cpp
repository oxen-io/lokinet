#include "ip_range.hpp"

#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace llarp
{
    static auto logcat = log::Cat("iprange");

    template <bool is_ipv4>
    static std::conditional_t<is_ipv4, ipv4_net, ipv6_net> parse_ip_net(
        std::string_view address, std::optional<uint8_t> default_mask)
    {
        auto mask_pos = address.find('/');
        std::string addr{address.substr(0, mask_pos)};
        quic::Address a{addr, 0};
        if (a.is_ipv4() != is_ipv4)
            throw std::invalid_argument{"Cannot construct an IPv{} range from a IPv{} address"_format(
                is_ipv4 ? "4" : "6", is_ipv4 ? "6" : "4")};

        uint8_t mask;
        if (mask_pos == std::string::npos)
        {
            if (default_mask)
                mask = *default_mask;
            else
                throw std::invalid_argument{"Invalid IP range: /N network mask is required"};
        }
        else if (!parse_int(address.substr(mask_pos + 1), mask) || mask > (is_ipv4 ? 32 : 128))
        {
            throw std::invalid_argument{"Invalid IP range: {} is not a valid IPv{} network mask"_format(
                address.substr(mask_pos), is_ipv4 ? "4" : "6")};
        }

        if constexpr (is_ipv4)
            return {a.to_ipv4(), mask};
        else
            return {a.to_ipv6(), mask};
    }

    ipv4_net parse_ipv4_net(std::string_view address, std::optional<uint8_t> default_mask)
    {
        return parse_ip_net<true>(address, default_mask);
    }
    ipv6_net parse_ipv6_net(std::string_view address, std::optional<uint8_t> default_mask)
    {
        return parse_ip_net<false>(address, default_mask);
    }
    ipv4_range parse_ipv4_range(std::string_view address, std::optional<uint8_t> default_mask)
    {
        return parse_ipv4_net(address, default_mask).to_range();
    }
    ipv6_range parse_ipv6_range(std::string_view address, std::optional<uint8_t> default_mask)
    {
        return parse_ipv6_net(address, default_mask).to_range();
    }

    // Encoded as ABCDM for each byte of A.B.C.D/M
    std::string encode(const ipv4_range& r)
    {
        std::string enc;
        enc.resize(5);
        oxenc::write_host_as_big(r.ip.addr, enc.data());
        enc[4] = static_cast<char>(r.mask);
        return enc;
    }
    ipv4_range decode_ipv4_range(std::string_view encoded)
    {
        if (encoded.size() != 5)
            throw std::invalid_argument{"Invalid ipv4 range encoding"};
        ipv4_range result;
        result.mask = static_cast<uint8_t>(encoded[4]);
        if (result.mask > 32)
            throw std::invalid_argument{"Invalid ipv4 range encoded netmask"};
        result.ip.addr = oxenc::load_big_to_host<uint32_t>(encoded.data());
        return result;
    }
    // Encode as *either* the full IPv6 address (16 bytes) followed by the netmask byte, or the
    // first 8 bytes of the IPv6 address followed by the netmask byte.  In the latter case, the
    // lower 8 bytes are implied to be 0.
    std::string encode(const ipv6_range& r)
    {
        std::string enc;
        enc.resize(r.ip.lo ? 17 : 9);
        oxenc::write_host_as_big(r.ip.hi, enc.data());
        if (r.ip.lo)
            oxenc::write_host_as_big(r.ip.lo, enc.data() + 8);
        enc.back() = static_cast<char>(r.mask);
        return enc;
    }
    ipv6_range decode_ipv6_range(std::string_view encoded)
    {
        if (encoded.size() != 9 && encoded.size() != 17)
            throw std::invalid_argument{"Invalid ipv6 range encoding"};
        ipv6_range result;
        result.mask = static_cast<uint8_t>(encoded.back());
        if (result.mask > 128)
            throw std::invalid_argument{"Invalid ipv6 range encoded netmask"};
        result.ip.hi = oxenc::load_big_to_host<uint64_t>(encoded.data());
        result.ip.lo = encoded.size() == 17 ? oxenc::load_big_to_host<uint64_t>(encoded.data() + 8) : 0;
        return result;
    }
    std::variant<ipv4_range, ipv6_range> decode_ip_range(std::string_view encoded)
    {
        switch (encoded.size())
        {
            case 5:
                return decode_ipv4_range(encoded);
            case 9:
            case 17:
                return decode_ipv6_range(encoded);
            default:
                throw std::invalid_argument{"Invalid encoded ip range"};
        }
    }

    ipv4_net to_ipv4_net(const quic::Address& addr, uint8_t mask)
    {
        if (!addr.is_ipv4())
            throw std::invalid_argument{"Cannot construct an ipv4_net from an IPv6 address"};
        if (mask > 32)
            throw std::invalid_argument{"{} is not a valid IPv4 network mask"_format(mask)};
        return {addr.to_ipv4(), mask};
    }
    ipv6_net to_ipv6_net(quic::Address addr, uint8_t mask)
    {
        if (!addr.is_ipv6())
            throw std::invalid_argument{"Cannot construct an ipv6_net from an IPv4 address"};
        if (mask > 128)
            throw std::invalid_argument{"{} is not a valid IPv6 network mask"_format(mask)};
        return {addr.to_ipv6(), mask};
    }

    static std::optional<ipv4_net> find_ipv4_net(
        uint32_t start, uint32_t end, uint8_t mask, const std::vector<ipv4_range>& exclude)
    {
        uint32_t step = uint32_t{1} << (32 - mask);
        auto ret = std::make_optional<ipv4_net>();
        auto& net = *ret;
        auto& addr = net.ip.addr;
        ret->mask = mask;
        for (addr = start + 1; addr < end; addr += step)
            if (std::ranges::none_of(
                    exclude, [&net](const auto& e) { return e.contains(net.ip) || net.contains(e.ip); }))
                return ret;
        ret.reset();
        return ret;
    }

    std::optional<ipv4_net> find_private_ipv4_net(const std::vector<ipv4_range>& exclude, uint8_t mask)
    {
        if (mask < 8)
        {
            log::warning(logcat, "Cannot auto-detect IPv4 private networks larger than /8");
            return std::nullopt;
        }

        // Cut off the smallest block we search to /20 (4096 addresses) just so that this doesn't
        // take an excessive amount of time if there are no free address spaces.  We still give back
        // whatever you requested, if smaller, we just don't search the smaller spaces.  That is, if
        // you request a /24 and 10.0.0.0/24 is already assigned then the next thing we would try is
        // 10.0.16.0/24 rather than 10.0.1.0/24.  (There are 4368 distinct private range /20
        // blocks).
        auto search_mask = std::min<uint8_t>(18, mask);
        std::optional<ipv4_net> ret;
        // Start looking in the 172.16.x.x - 172.31.x.x range first as it is the least commonly
        // used, then 10.x.x.x, then finally the tiny 192.168.x.x range.
        if (mask >= 12)
            ret = find_ipv4_net((172 << 24) | (16 << 16), (172 << 24) | (32 << 16), search_mask, exclude);

        if (!ret && mask >= 8)
            ret = find_ipv4_net((10 << 24), (11 << 24), search_mask, exclude);

        if (!ret && mask >= 16)
            ret = find_ipv4_net((192 << 24) | (168 << 16), (192 << 24) | (169 << 16), search_mask, exclude);

        if (ret)
            ret->mask = mask;  // In case we used a larger search_mask

        return ret;
    }

    std::optional<ipv6_net> find_private_ipv6_net(const std::vector<ipv6_range>& exclude, uint8_t mask)
    {
        // This /48 is registered for Lokinet in the IPv6 ULA registry (https://ula.ungleich.ch/):
        constexpr uint64_t start = 0xfd2e'6c6f'6b69'0000;  // 2e 6c 6f 6b 69 == . l o k i
        constexpr uint64_t end = start + 0x1'0000;

        if (mask < 48)
        {
            log::warning(logcat, "Cannot auto-detect IPv6 private networks larger than /48");
            return std::nullopt;
        }

        // First do a preliminary check that none of the excluded ranges covers the entire /48,
        // because if so, we simply can't succeed.
        {
            ipv6 min, max;
            min.hi = start;
            max.hi = end - 1;
            max.lo = 0xffff'ffff'ffff'ffff;
            if (std::ranges::any_of(
                    exclude, [&min, &max](const auto& e) { return e.contains(min) && e.contains(max); }))
            {
                log::warning(
                    logcat, "Failed to find a free private /64 IPv6 range: the entire {}/48 range is unavailable", min);
                return std::nullopt;
            }
        }

        // Cut off the smallest block we search to /64 just so that this doesn't take an excessive
        // amount of time, and because we can do it with just uint64_t math.  We still give back
        // whatever you requested, if smaller, we just don't search the smaller spaces.
        auto search_mask = std::min<uint8_t>(64, mask);
        uint64_t hi_step = 1 << (64 - search_mask);

        auto ret = std::make_optional<ipv6_net>();
        auto& net = *ret;
        net.ip.lo = 1;
        auto& hi = net.ip.hi;
        for (hi = start; hi < end; hi += hi_step)
        {
            if (std::ranges::none_of(
                    exclude, [&net](const auto& e) { return e.contains(net.ip) || net.contains(e.ip); }))
            {
                ret->mask = mask;  // In case we enlarged it for searching
                return ret;
            }
        }

        ret.reset();
        return ret;
    }

}  //  namespace llarp

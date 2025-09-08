#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace llarp
{

    // Helper shims to convert a uint8_t span into a std::byte span of the same size/extent, and
    // vice versa.

    template <std::size_t Extent = std::dynamic_extent>
    std::span<std::byte, Extent> as_bspan(std::span<uint8_t, Extent> s)
    {
        return std::span<std::byte, Extent>{reinterpret_cast<std::byte*>(s.data()), s.size()};
    }
    template <std::size_t Extent = std::dynamic_extent>
    std::span<const std::byte, Extent> as_bspan(std::span<const uint8_t, Extent> s)
    {
        return std::span<const std::byte, Extent>{reinterpret_cast<const std::byte*>(s.data()), s.size()};
    }

    template <std::size_t Extent = std::dynamic_extent>
    std::span<std::uint8_t, Extent> as_uspan(std::span<std::byte, Extent> s)
    {
        return std::span<std::uint8_t, Extent>{reinterpret_cast<std::uint8_t*>(s.data()), s.size()};
    }
    template <std::size_t Extent = std::dynamic_extent>
    std::span<const std::uint8_t, Extent> as_uspan(std::span<const std::byte, Extent> s)
    {
        return std::span<const std::uint8_t, Extent>{reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
    }

    // Convert string_view into const byte span:
    inline std::span<const std::byte> as_bspan(std::string_view s)
    {
        return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
    }

    // Convert string into byte span:
    inline std::span<std::byte> as_bspan(std::string& s) { return {reinterpret_cast<std::byte*>(s.data()), s.size()}; }

    namespace detail
    {
        inline constexpr std::span<std::byte> split_span(std::byte*& data, size_t n)
        {
            std::span<std::byte> ret{data, n};
            data += n;
            return ret;
        }
        template <size_t Len>
        constexpr std::span<std::byte, Len> split_span(std::byte*& data)
        {
            std::span<std::byte, Len> ret{data, Len};
            data += Len;
            return ret;
        }
    }  // namespace detail

    // Split a span into subspans.  Takes n length values and returns an array of n+1 spans (the
    // last contains everything beyond the last specified size).  Does *NOT* check length, you must
    // ensure the input is no shorter than the sum of the input sizes!
    //
    // e.g. split_span(x, 1, 2) with x set to "abcdef" would give ["a", "bc", "def"].
    template <std::convertible_to<size_t>... Lengths>
        requires(sizeof...(Lengths) > 0)
    constexpr std::array<std::span<std::byte>, sizeof...(Lengths) + 1> split_span(
        std::span<std::byte> input, Lengths... n)
    {
        auto* data = input.data();
        return {detail::split_span(data, n)..., {data, (input.size() - ... - n)}};
    }

    // Similar to split_span, except that it takes the 2nd through Nth sizes, and leaves the *first*
    // size as dynamic rather than that last.  e.g. split_span_tail(x, 1, 2) with x set to
    // "abcdef" would give ["abc", "d", "ef"].
    template <std::convertible_to<size_t>... Lengths>
    constexpr std::array<std::span<std::byte>, sizeof...(Lengths) + 1> split_span_tail(
        std::span<std::byte> input, Lengths... n)
    {
        auto* data = input.data() + (input.size() - ... - n);
        return {input.first((input.size() - ... - n)), detail::split_span(data, n)...};
    }

    // Similar to the above, but takes sizes as template arguments returning fixed-size spans
    // (except for the tail piece).  The length of the input span is not checked and must be at
    // least as large as the sum of the input lengths!
    template <size_t... Lengths>
        requires(sizeof...(Lengths) > 0)
    constexpr std::tuple<std::span<std::byte, Lengths>..., std::span<std::byte>> split_span(std::span<std::byte> input)
    {
        auto* data = input.data();
        return {detail::split_span<Lengths>(data)..., {data, (input.size() - ... - Lengths)}};
    }

    // Same as split_span_tail, except that it takes the 2nd through Nth sizes as template arguments
    // and those elements in the returned tuple are fixed size spans of the requested sizes.  Input
    // must be long enough to satisfy the requested sizes (this is not checked).
    template <size_t... Lengths>
        requires(sizeof...(Lengths) > 0)
    constexpr std::tuple<std::span<std::byte>, std::span<std::byte, Lengths>...> split_span_tail(
        std::span<std::byte> input)
    {
        auto* data = input.data() + (input.size() - ... - Lengths);
        return {input.first((input.size() - ... - Lengths)), detail::split_span<Lengths>(data)...};
    }

}  // namespace llarp

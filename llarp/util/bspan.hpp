#pragma once

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

}  // namespace llarp

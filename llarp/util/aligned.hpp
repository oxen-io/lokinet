#pragma once

#include <oxenc/base32z.h>
#include <oxenc/bt.h>
#include <oxenc/bt_serialize.h>
#include <oxenc/hex.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace llarp
{
    /// aligned buffer that is sz bytes long and aligns to the nearest Alignment
    template <size_t sz>
    struct alignas(8) AlignedBuffer
    {
        static_assert(sz % 8 == 0, "AlignedBuffer cannot be used with buffers that aren't a multiple of 8");

        static constexpr size_t SIZE = sz;

        AlignedBuffer() { zero(); }

        explicit AlignedBuffer(std::span<const uint8_t, SIZE> buf) { *this = buf; }
        explicit AlignedBuffer(std::span<const std::byte, SIZE> buf) { *this = buf; }

        AlignedBuffer& operator=(std::span<const uint8_t, SIZE> buf)
        {
            assign(buf);
            return *this;
        }
        AlignedBuffer& operator=(std::span<const std::byte, SIZE> buf)
        {
            assign(buf);
            return *this;
        }
        // Assigns to the aligned buffer contents from a spannable input of the same size
        void assign(std::span<const uint8_t, SIZE> buf) { std::memcpy(_data.data(), buf.data(), SIZE); }
        void assign(std::span<const std::byte, SIZE> buf) { std::memcpy(_data.data(), buf.data(), SIZE); }

        // Copies the aligned buffer contents into a writeable span of the same size
        void copy_to(std::span<std::byte, SIZE> buf) const { std::memcpy(buf.data(), _data.data(), SIZE); }
        void copy_to(std::span<uint8_t, SIZE> buf) const { std::memcpy(buf.data(), _data.data(), SIZE); }

        /// bitwise NOT
        AlignedBuffer<sz> operator~() const
        {
            AlignedBuffer<sz> ret;
            std::transform(begin(), end(), ret.begin(), [](uint8_t a) { return ~a; });

            return ret;
        }

        auto operator<=>(const AlignedBuffer& other) const = default;
        bool operator==(const AlignedBuffer& other) const = default;

        AlignedBuffer operator^(const AlignedBuffer& other) const
        {
            AlignedBuffer<sz> ret;
            std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>());
            return ret;
        }

        AlignedBuffer& operator^=(const AlignedBuffer& other)
        {
            // Mutate in place instead.
            for (size_t i = 0; i < sz; ++i)
            {
                _data[i] ^= other._data[i];
            }
            return *this;
        }

        uint8_t& operator[](size_t idx)
        {
            assert(idx < SIZE);
            return _data[idx];
        }

        const uint8_t& operator[](size_t idx) const
        {
            assert(idx < SIZE);
            return _data[idx];
        }

        static constexpr size_t size() { return sz; }

        void Fill(uint8_t f) { _data.fill(f); }

        std::array<uint8_t, SIZE>& as_array() { return _data; }

        const std::array<uint8_t, SIZE>& as_array() const { return _data; }

        uint8_t* data() { return _data.data(); }

        const uint8_t* data() const { return _data.data(); }

        std::span<std::byte, SIZE> span()
        {
            return std::span<std::byte, SIZE>{reinterpret_cast<std::byte*>(_data.data()), SIZE};
        }
        std::span<const std::byte, SIZE> span() const
        {
            return std::span<const std::byte, SIZE>{reinterpret_cast<const std::byte*>(_data.data()), SIZE};
        }

        // Implicit conversion to span
        operator std::span<std::byte, SIZE>() { return span(); }
        operator std::span<std::byte>() { return span(); }
        operator std::span<const std::byte, SIZE>() const { return span(); }
        operator std::span<const std::byte>() const { return span(); }

        // Shortcut for .span().first/last:
        std::span<std::byte> first(size_t n) { return span().first(n); }
        template <size_t N>
            requires(N <= SIZE)
        std::span<std::byte, N> first()
        {
            return span().template first<N>();
        }
        std::span<std::byte> last(size_t n) { return span().last(n); }
        template <size_t N>
            requires(N <= SIZE)
        std::span<std::byte, N> last()
        {
            return span().template last<N>();
        }

        bool is_zero() const
        {
            const auto* ptr = reinterpret_cast<const uint64_t*>(data());
            for (size_t idx = 0; idx < SIZE / sizeof(uint64_t); idx++)
            {
                if (ptr[idx])
                    return false;
            }
            return true;
        }

        void zero() { _data.fill(0); }

        typename std::array<uint8_t, SIZE>::iterator begin() { return _data.begin(); }

        typename std::array<uint8_t, SIZE>::iterator end() { return _data.end(); }

        typename std::array<uint8_t, SIZE>::const_iterator begin() const { return _data.cbegin(); }

        typename std::array<uint8_t, SIZE>::const_iterator end() const { return _data.cend(); }

        bool from_base32z(std::string_view b32z)
        {
            if (b32z.size() != oxenc::to_base32z_size(sz) || !oxenc::is_base32z(b32z))
                return false;
            oxenc::from_base32z(b32z.begin(), b32z.end(), _data.begin());
            return true;
        }

        std::string bt_encode() const { return oxenc::bt_serialize(_data); }

        bool bt_decode(std::string buf)
        {
            oxenc::bt_deserialize(buf, _data);
            return true;
        }

        std::string_view to_view() const { return {reinterpret_cast<const char*>(data()), sz}; }

        std::string ToHex() const { return oxenc::to_hex(begin(), end()); }

        bool FromHex(std::string_view str)
        {
            if (str.size() != 2 * size() || !oxenc::is_hex(str))
                return false;
            oxenc::from_hex(str.begin(), str.end(), begin());
            return true;
        }

        std::string to_string() const { return ToHex(); }
        static constexpr bool to_string_formattable = true;

        // Deferred conversion object meant for log statements to be able to log a shortened b32z
        // value without needing to do the conversion when the log statement is skipped.
        struct short_log_printer
        {
            const AlignedBuffer<sz>& buf;
            std::string to_string() const { return oxenc::to_base32z(buf.begin(), buf.begin() + 5); }
            static constexpr bool to_string_formattable = true;
        };
        // Used in log statements to log the value as its first 8 base32z characters:
        short_log_printer short_string() const { return {*this}; }

        template <typename T>
            requires(std::derived_from<T, AlignedBuffer<T::SIZE>>)
        static T filled(uint8_t f)
        {
            T ret;
            ret.Fill(f);
            return ret;
        }

      private:
        std::array<uint8_t, SIZE> _data;
    };

    static_assert(sizeof(AlignedBuffer<32>) == 32, "AlignedBuffer should have no overhead");
    static_assert(sizeof(AlignedBuffer<24>) == 24, "AlignedBuffer should have no overhead");
    static_assert(sizeof(AlignedBuffer<8>) == 8, "AlignedBuffer should have no overhead");

    struct AlignedHasher
    {
        // Hashing implementation that uses the raw data value held in an AlignedBuffer-derived
        // class as the hash value.  This is only suitable for values that come from hashes or
        // pubkeys where values are unlikely to be correlated.
        template <typename T>
            requires std::is_base_of_v<AlignedBuffer<sizeof(T)>, T>
        std::size_t operator()(const T& buf) const noexcept
        {
            if constexpr (alignof(T) >= sizeof(size_t))
                return *reinterpret_cast<const size_t*>(buf.data());
            else
            {
                std::size_t h;
                static_assert(T::SIZE >= sizeof(h));
                std::memcpy(&h, buf.data(), sizeof(h));
                return h;
            }
        }
    };

}  // namespace llarp

namespace std
{
    template <size_t sz>
    struct hash<llarp::AlignedBuffer<sz>> : llarp::AlignedHasher
    {};
}  // namespace std

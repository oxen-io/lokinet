#pragma once

#include <llarp/net/policy.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp
{
    struct alignas(uint64_t) session_tag
    {
        inline static constexpr size_t SIZE = 8;

      private:
        std::array<std::byte, SIZE> buf;

      public:
        session_tag() = default;
        explicit session_tag(protocol_flag protocols);
        explicit session_tag(std::span<const std::byte, SIZE> buf);

        protocol_flag protocols() const { return static_cast<protocol_flag>(buf[0]); }

        void assign(std::span<const std::byte, SIZE> buf);

        std::string_view view() const;

        std::span<const std::byte, SIZE> span() const { return buf; }

        constexpr size_t size() const { return buf.size(); }

        bool operator==(const session_tag& other) const { return buf == other.buf; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::session_tag>
    {
        size_t operator()(const llarp::session_tag& tag) const noexcept
        {
            return std::hash<std::string_view>{}(tag.view());
        }
    };
}  // namespace std

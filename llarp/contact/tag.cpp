#include "tag.hpp"

#include <llarp/net/policy.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <sodium/randombytes.h>

namespace llarp
{
    static auto logcat = log::Cat("session-tag");

    session_tag::session_tag(protocol_flag protocols)
    {
        protocols &= proto_mask;
        randombytes_buf(buf.data() + 1, buf.size() - 1);
        buf[0] = static_cast<std::byte>(protocols);
        log::trace(logcat, "new session tag generated: {}", buffer_printer{buf});
    }

    session_tag::session_tag(std::span<const std::byte, SIZE> data) { assign(data); }

    void session_tag::assign(std::span<const std::byte, SIZE> data)
    {
        std::memcpy(buf.data(), data.data(), data.size());
    }

    std::string_view session_tag::view() const { return {reinterpret_cast<const char*>(buf.data()), buf.size()}; }

    std::string session_tag::to_string() const { return oxenc::to_hex(buf.begin(), buf.end()); }

}  // namespace llarp

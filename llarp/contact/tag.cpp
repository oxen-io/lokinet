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
        buf[0] = static_cast<uint8_t>(protocols);
        log::trace(logcat, "new session tag generated: {}", buffer_printer{buf});
    }

    void session_tag::read(std::string_view data)
    {
        if (data.size() != buf.size())
            throw std::invalid_argument{
                "Buffer size mismatch (received: {}, expected: {}) reading in session tag!"_format(
                    data.size(), buf.size())};

        std::memcpy(buf.data(), data.data(), data.size());
    }

    std::string_view session_tag::view() const { return {reinterpret_cast<const char*>(buf.data()), buf.size()}; }

    std::span<const unsigned char> session_tag::span() const { return buf; }

    std::string session_tag::to_string() const { return oxenc::to_hex(buf.begin(), buf.end()); }

}  // namespace llarp

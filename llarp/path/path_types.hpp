#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/constants.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/aligned.hpp>

namespace oxen::quic
{
    struct message;
}

namespace llarp
{
    struct HopID final : public AlignedBuffer<PATHIDSIZE>
    {
        using AlignedBuffer<PATHIDSIZE>::AlignedBuffer;

        static HopID make_random()
        {
            HopID h;
            randombytes_buf(h.data(), h.size());
            return h;
        }
    };

    namespace handlers
    {
        class SessionEndpoint;
    }

    struct session_path_interface
    {
        virtual void send_path_control_message(
            std::string_view method,
            std::span<const std::byte> payload,
            std::function<void(quic::message)> func,
            std::byte type = std::byte{0x01}) = 0;

        virtual void send_path_data_message(
            std::vector<std::byte>&& body,
            SymmNonce&& nonce = SymmNonce::make_random(),
            std::byte type = std::byte{0x01}) = 0;

        virtual RouterID terminal_rid() const = 0;
        virtual HopID terminal_hopid() const = 0;

        virtual std::string to_string() const = 0;
        static constexpr bool to_string_formattable = true;

        virtual ~session_path_interface() = default;
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::HopID> : hash<llarp::AlignedBuffer<llarp::HopID::SIZE>>
    {};
}  // namespace std

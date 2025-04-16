#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/constants.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/link/types.hpp>
#include <llarp/util/aligned.hpp>

namespace llarp
{
    struct HopID final : public AlignedBuffer<PATHIDSIZE>
    {
        using AlignedBuffer<PATHIDSIZE>::AlignedBuffer;

        static HopID make_random()
        {
            HopID h;
            h.Randomize();
            return h;
        }
    };

    namespace handlers
    {
        class SessionEndpoint;
    }

    struct session_path_interface
    {
        virtual void link_session(session_tag t) = 0;
        virtual bool unlink_session(session_tag t) = 0;
        virtual bool is_linked() const = 0;

        virtual bool send_path_control_message(std::string method, std::string body, bt_control_response_hook func) = 0;
        virtual bool send_path_data_message(std::string body) = 0;

        virtual RouterID terminal_rid() const = 0;
        virtual HopID terminal_txid() const = 0;

        virtual handlers::SessionEndpoint& parent() = 0;

        virtual std::string to_string() const = 0;
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::HopID> : hash<llarp::AlignedBuffer<llarp::HopID::SIZE>>
    {};
}  // namespace std

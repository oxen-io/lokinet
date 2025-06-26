#pragma once

#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>

#include <oxenc/bt_producer.h>

namespace llarp::messages
{
    inline constexpr auto STATUS_KEY = "!"sv;
    std::string serialize_status_response(std::string_view value);

    extern const std::string TIMEOUT_RESPONSE;
    extern const std::string ERROR_RESPONSE;
    extern const std::string OK_RESPONSE;

}  // namespace llarp::messages

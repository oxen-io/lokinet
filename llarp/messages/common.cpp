#include "common.hpp"

namespace llarp::messages
{

    std::string serialize_status_response(std::string_view value)
    {
        oxenc::bt_dict_producer p;
        p.append(STATUS_KEY, value);
        return std::move(p).str();
    }

    const std::string TIMEOUT_RESPONSE = serialize_status_response("TIMEOUT");
    const std::string ERROR_RESPONSE = serialize_status_response("ERROR");
    const std::string OK_RESPONSE = serialize_status_response("OK");

}  // namespace llarp::messages

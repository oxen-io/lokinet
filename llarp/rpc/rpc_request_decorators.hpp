#pragma once

#include "json_binary_proxy.hpp"
#include "json_bt.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string_view>
#include <llarp/config/config.hpp>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <oxen/log/omq_logger.hpp>

namespace tools
{
  //  Type wrapper that contains an arbitrary list of types.
  template <typename...>
  struct type_list
  {};
}  // namespace tools

namespace llarp::rpc
{
  //  Base class that all RPC requests will expand for each endpoint type
  struct RPCRequest
  {
   private:
    bool bt = false;

   public:
    //  Returns true if response is bt-encoded, and false for json
    //  Note: do not set value
    bool
    is_bt() const
    {
      return bt;
    }

    //  Callable method to indicate request is bt-encoded
    void
    set_bt()
    {
      bt = true;
      response_b64.format = llarp::rpc::json_binary_proxy::fmt::bt;
      response_hex.format = llarp::rpc::json_binary_proxy::fmt::bt;
    }

    //  Invoked if this.replier is still present. If it is "stolen" by endpoint (moved from
    //  RPC struct), then endpoint handles sending reply
    void
    send_response()
    {
      replier->reply(
          is_bt() ? oxenc::bt_serialize(json_to_bt(std::move(response))) : response.dump());
    }

    void
    send_response(nlohmann::json _response)
    {
      response = std::move(_response);
      send_response();
    }

    //  Response Data:
    //  bt-encoded are converted in real-time
    //    - bool becomes 0 or 1
    //    - key:value where value == null are omitted
    //    - other nulls will raise an exception if found in json
    //    - no doubles
    //      - to store doubles: encode bt in endpoint-specific way
    //    - binary strings will fail json serialization; caller must
    //
    //        std::string binary = some_binary_data();
    //        request.response["binary_value"] = is_bt ? binary : oxenmq::to_hex(binary)
    //
    nlohmann::json response;

    //  Proxy Object:
    //  Sets binary data in "response"
    //    - if return type is json, encodes as hex
    //    - if return type is bt, then binary is untouched
    //
    //  Usage:
    //    std::string data = "abc";
    //    request.response_hex["foo"]["bar"] = data; // json: "616263", bt: "abc"
    //
    llarp::rpc::json_binary_proxy response_hex{response, llarp::rpc::json_binary_proxy::fmt::hex};

    //  Proxy Object:
    //  Encodes binary data as base_64 for json-encoded responses, leaves as binary for bt-encoded
    //  responses
    //
    //    Usage:
    //      std::string data = "abc"
    //      request.response_b64["foo"]["bar"] = data; json: "YWJj", bt: "abc"
    //
    llarp::rpc::json_binary_proxy response_b64{
        response, llarp::rpc::json_binary_proxy::fmt::base64};

    //  The oxenmq deferred send object into which the response will be set.  If this optional is
    //  still set when the `invoke` call returns then the response is sent at that point; if it has
    //  been moved out (i.e. either just this instance or the whole request struct is stolen/moved
    //  by the invoke function) then it is the invoke function's job to send a reply.  Typically
    //  this is done when a response cannot be sent immediately
    std::optional<oxenmq::Message::DeferredSend> replier;
  };

  //  Tag types that are inherited to set RPC endpoint properties

  //  RPC call wil take no input arguments
  //    Parameter dict can be passed, but will be ignored
  struct NoArgs : virtual RPCRequest
  {};

  // RPC call will be executed immediately
  struct Immediate : virtual RPCRequest
  {};

}  // namespace llarp::rpc

#pragma once

#include "rpc_server.hpp"
#include "rpc_request_parser.hpp"
#include "rpc_request_decorators.hpp"
#include "rpc_request_definitions.hpp"
#include "json_bt.hpp"
#include <string_view>
#include <llarp/config/config.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <oxen/log/omq_logger.hpp>

namespace llarp::rpc
{

  using nlohmann::json;

  template <typename RPC>
  auto
  make_invoke()
  {
    return [](oxenmq::Message& m, RPCServer& server) {
      EndpointHandler<RPC> handler{server, m.send_later()};
      auto& rpc = handler.rpc;

      if (m.data.size() > 1)
        m.send_reply(CreateJSONError(
            "Bad Request: RPC requests must have at most one data part (received {})"_format(
                m.data.size())));

      // parsing input as bt or json
      //    hand off to parse_request (overloaded versions)
      try
      {
        if (m.data.empty() or m.data[0].empty())
        {
          parse_request(rpc, nlohmann::json::object());
        }
        else if (m.data[0].front() == 'd')
        {
          rpc.set_bt();
          parse_request(rpc, oxenc::bt_dict_consumer{m.data[0]});
        }
        else
        {
          parse_request(rpc, nlohmann::json::parse(m.data[0]));
        }
      }
      catch (const std::exception& e)
      {
        m.send_reply(CreateJSONError("Failed to parse request parameters: "s + e.what()));
        return;
      }

      if (not std::is_base_of_v<Immediate, RPC>)
      {
        server.m_Router.loop()->call_soon(std::move(handler));
      }
      else
      {
        handler();
      }
    };
  }

}  // namespace llarp::rpc

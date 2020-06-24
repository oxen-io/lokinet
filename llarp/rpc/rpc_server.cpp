#include "rpc_server.hpp"
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>
#include <constants/version.hpp>
#include <nlohmann/json.hpp>
#include <net/ip_range.hpp>
#include <service/context.hpp>
#include <service/auth.hpp>

namespace llarp::rpc
{
  RpcServer::RpcServer(LMQ_ptr lmq, AbstractRouter* r) : m_LMQ(std::move(lmq)), m_Router(r)
  {
  }

  /// maybe parse json from message paramter at index
  std::optional<nlohmann::json>
  MaybeParseJSON(const lokimq::Message& msg, size_t index = 0)
  {
    try
    {
      const auto& str = msg.data.at(index);
      return nlohmann::json::parse(str);
    }
    catch (std::exception&)
    {
      return std::nullopt;
    }
  }

  template <typename Result_t>
  std::string
  CreateJSONResponse(Result_t result)
  {
    const auto obj = nlohmann::json{
        {"error", nullptr},
        {"result", result},
    };
    return obj.dump();
  }

  std::string
  CreateJSONError(std::string_view msg)
  {
    const auto obj = nlohmann::json{
        {"error", msg},
    };
    return obj.dump();
  }

  /// a function that replies to an rpc request
  using ReplyFunction_t = std::function<void(std::string)>;

  void
  HandleJSONRequest(
      lokimq::Message& msg, std::function<void(nlohmann::json, ReplyFunction_t)> handleRequest)
  {
    const auto maybe = MaybeParseJSON(msg);
    if (not maybe.has_value())
    {
      msg.send_reply(CreateJSONError("failed to parse json"));
      return;
    }
    if (not maybe->is_object())
    {
      msg.send_reply(CreateJSONError("request data not a json object"));
      return;
    }
    try
    {
      std::promise<std::string> reply;
      handleRequest(*maybe, [&reply](std::string result) { reply.set_value(result); });
      auto ftr = reply.get_future();
      msg.send_reply(ftr.get());
    }
    catch (std::exception& ex)
    {
      msg.send_reply(CreateJSONError(ex.what()));
    }
  }

  void
  RpcServer::AsyncServeRPC(lokimq::address url)
  {
    m_LMQ->listen_plain(url.zmq_address());
    m_LMQ->add_category("llarp", lokimq::AuthLevel::none)
        .add_command(
            "halt",
            [&](lokimq::Message& msg) {
              if (not m_Router->IsRunning())
              {
                msg.send_reply(CreateJSONError("router is not running"));
                return;
              }
              m_Router->Stop();
              msg.send_reply(CreateJSONResponse("OK"));
            })
        .add_request_command(
            "version",
            [](lokimq::Message& msg) { msg.send_reply(CreateJSONResponse(llarp::VERSION_FULL)); })
        .add_request_command(
            "status",
            [&](lokimq::Message& msg) {
              std::promise<util::StatusObject> result;
              LogicCall(m_Router->logic(), [&result, r = m_Router]() {
                const auto state = r->ExtractStatus();
                result.set_value(state);
              });
              auto ftr = result.get_future();
              msg.send_reply(CreateJSONResponse(ftr.get()));
            })
        .add_request_command("exit", [&](lokimq::Message& msg) {
          HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
            std::optional<service::Address> exit;
            IPRange range;
            bool map = true;
            const auto exit_itr = obj.find("exit");
            if (exit_itr != obj.end())
            {
              service::Address addr;
              if (not addr.FromString(exit_itr->get<std::string>()))
              {
                reply(CreateJSONError("invalid exit address"));
                return;
              }
              exit = addr;
            }

            const auto unmap_itr = obj.find("unmap");
            if (unmap_itr != obj.end() and unmap_itr->get<bool>())
            {
              map = false;
            }

            const auto range_itr = obj.find("range");
            if (range_itr == obj.end())
            {
              range.FromString("0.0.0.0/0");
            }
            else if (not range.FromString(range_itr->get<std::string>()))
            {
              reply(CreateJSONError("invalid ip range"));
              return;
            }
            std::optional<std::string> token;
            const auto token_itr = obj.find("token");
            if (token_itr != obj.end())
            {
              token = token_itr->get<std::string>();
            }

            std::string endpoint = "default";
            const auto endpoint_itr = obj.find("endpoint");
            if (endpoint_itr != obj.end())
            {
              endpoint = endpoint_itr->get<std::string>();
            }
            std::promise<std::string> result;
            LogicCall(r->logic(), [map, exit, range, token, endpoint, r, &result]() {
              auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
              if (ep == nullptr)
              {
                result.set_value(CreateJSONError("no endpoint with name " + endpoint));
                return;
              }
              if (map and exit.has_value())
              {
                ep->MapExitRange(range, *exit);
                if (token.has_value())
                {
                  ep->SetAuthInfoForEndpoint(*exit, service::AuthInfo{*token});
                }
              }
              else if (map and not exit.has_value())
              {
                result.set_value(CreateJSONError("no exit address provided"));
                return;
              }
              else if (not map)
              {
                ep->UnmapExitRange(range);
              }
              result.set_value(CreateJSONResponse("OK"));
            });
            auto ftr = result.get_future();
            reply(ftr.get());
          });
        });
  }
}  // namespace llarp::rpc

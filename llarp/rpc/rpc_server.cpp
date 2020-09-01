#include "rpc_server.hpp"
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>
#include <constants/version.hpp>
#include <nlohmann/json.hpp>
#include <net/ip_range.hpp>
#include <net/route.hpp>
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
            [r = m_Router](lokimq::Message& msg) {
              util::StatusObject result{{"version", llarp::VERSION_FULL},
                                        {"uptime", to_json(r->Uptime())}};
              msg.send_reply(CreateJSONResponse(result));
            })
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
        .add_request_command(
            "exit",
            [&](lokimq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (r->IsServiceNode())
                {
                  reply(CreateJSONError("not supported"));
                  return;
                }
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
                LogicCall(r->logic(), [map, exit, range, token, endpoint, r, reply]() {
                  auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
                  if (ep == nullptr)
                  {
                    reply(CreateJSONError("no endpoint with name " + endpoint));
                    return;
                  }
                  if (map and exit.has_value())
                  {
                    const auto gateways = net::GetGatewaysNotOnInterface(ep->GetIfName());
                    if (gateways.empty())
                    {
                      reply(CreateJSONError("no gateway found"));
                      return;
                    }
                    ep->MapExitRange(range, *exit);
                    if (token.has_value())
                    {
                      ep->SetAuthInfoForEndpoint(*exit, service::AuthInfo{*token});
                    }
                    ep->EnsurePathToService(
                        *exit,
                        [r, gateway = gateways[0], reply, ep](auto, service::OutboundContext* ctx) {
                          if (ctx == nullptr)
                          {
                            reply(CreateJSONError("could not find exit"));
                            return;
                          }
                          std::vector<std::string> firsthops;
                          r->ForEachPeer(
                              [&firsthops](const auto* link, bool) {
                                firsthops.emplace_back(link->GetRemoteEndpoint().toHost());
                              },
                              false);
                          for (const auto& hop : firsthops)
                          {
                            net::AddRoute(hop, gateway);
                          }
                          net::AddDefaultRouteViaInterface(ep->GetIfName());
                          r->SetDownHook([firsthops, gateway]() {
                            for (const auto& hop : firsthops)
                            {
                              net::DelRoute(hop, gateway);
                            }
                          });
                          reply(CreateJSONResponse("OK"));
                        },
                        5s);
                    return;
                  }
                  else if (map and not exit.has_value())
                  {
                    reply(CreateJSONError("no exit address provided"));
                    return;
                  }
                  else if (not map)
                  {
                    const auto gateways = net::GetGatewaysNotOnInterface(ep->GetIfName());
                    if (gateways.empty())
                    {
                      reply(CreateJSONError("no gateway found"));
                      return;
                    }
                    net::DelDefaultRouteViaInterface(ep->GetIfName());
                    r->ForEachPeer(
                        [gateway = gateways[0]](const auto* link, bool) {
                          net::DelRoute(link->GetRemoteEndpoint().toHost(), gateway);
                        },
                        false);

                    ep->UnmapExitRange(range);
                  }
                  reply(CreateJSONResponse("OK"));
                });
              });
            })
        .add_request_command("config", [&](lokimq::Message& msg) {
          HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
            {
              const auto itr = obj.find("override");
              if (itr != obj.end())
              {
                if (not itr->is_object())
                {
                  reply(CreateJSONError(stringify("override is not an object")));
                  return;
                }
                for (const auto& [section, value] : itr->items())
                {
                  if (not value.is_object())
                  {
                    reply(CreateJSONError(
                        stringify("failed to set [", section, "] section is not an object")));
                    return;
                  }
                  for (const auto& [key, value] : value.items())
                  {
                    if (not value.is_string())
                    {
                      reply(CreateJSONError(stringify(
                          "failed to set [", section, "]:", key, " value is not a string")));
                      return;
                    }
                    r->GetConfig()->Override(section, key, value.get<std::string>());
                  }
                }
              }
            }
            {
              const auto itr = obj.find("reload");
              if (itr != obj.end() and itr->get<bool>())
              {
                r->QueueDiskIO([conf = r->GetConfig()]() { conf->Save(); });
              }
            }
            reply(CreateJSONResponse("OK"));
          });
        });
  }
}  // namespace llarp::rpc

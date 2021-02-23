#include "rpc_server.hpp"
#include <router/route_poker.hpp>
#include <util/thread/logic.hpp>
#include <constants/version.hpp>
#include <nlohmann/json.hpp>
#include <net/ip_range.hpp>
#include <net/route.hpp>
#include <service/context.hpp>
#include <service/auth.hpp>
#include <service/name.hpp>
#include <router/abstractrouter.hpp>

namespace llarp::rpc
{
  RpcServer::RpcServer(LMQ_ptr lmq, AbstractRouter* r) : m_LMQ(std::move(lmq)), m_Router(r)
  {}

  /// maybe parse json from message paramter at index
  std::optional<nlohmann::json>
  MaybeParseJSON(const oxenmq::Message& msg, size_t index = 0)
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
      oxenmq::Message& msg, std::function<void(nlohmann::json, ReplyFunction_t)> handleRequest)
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
      handleRequest(
          *maybe, [defer = msg.send_later()](std::string result) { defer.reply(result); });
    }
    catch (std::exception& ex)
    {
      msg.send_reply(CreateJSONError(ex.what()));
    }
  }

  void
  RpcServer::AsyncServeRPC(oxenmq::address url)
  {
    m_LMQ->listen_plain(url.zmq_address());
    m_LMQ->add_category("llarp", oxenmq::AuthLevel::none)
        .add_command(
            "halt",
            [&](oxenmq::Message& msg) {
              if (not m_Router->IsRunning())
              {
                msg.send_reply(CreateJSONError("router is not running"));
                return;
              }
              msg.send_reply(CreateJSONResponse("OK"));
              m_Router->Stop();
            })
        .add_request_command(
            "version",
            [r = m_Router](oxenmq::Message& msg) {
              util::StatusObject result{{"version", llarp::VERSION_FULL},
                                        {"uptime", to_json(r->Uptime())}};
              msg.send_reply(CreateJSONResponse(result));
            })
        .add_request_command(
            "status",
            [&](oxenmq::Message& msg) {
              LogicCall(m_Router->logic(), [defer = msg.send_later(), r = m_Router]() {
                std::string data;
                if (r->IsRunning())
                {
                  data = CreateJSONResponse(r->ExtractStatus());
                }
                else
                {
                  data = CreateJSONError("router not yet ready");
                }
                defer.reply(data);
              });
            })
        .add_request_command(
            "endpoint",
            [&](lokimq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (r->IsServiceNode())
                {
                  reply(CreateJSONError("not supported"));
                  return;
                }
                std::string endpoint = "default";
                std::unordered_set<service::Address> kills;
                {
                  const auto itr = obj.find("endpoint");
                  if (itr != obj.end())
                    endpoint = itr->get<std::string>();
                }
                {
                  const auto itr = obj.find("kill");
                  if (itr != obj.end())
                  {
                    if (itr->is_array())
                    {
                      for (auto kill_itr = itr->begin(); kill_itr != itr->end(); ++kill_itr)
                      {
                        if (kill_itr->is_string())
                          kills.emplace(kill_itr->get<std::string>());
                      }
                    }
                    else if (itr->is_string())
                    {
                      kills.emplace(itr->get<std::string>());
                    }
                  }
                }
                if (kills.empty())
                {
                  reply(CreateJSONError("no action taken"));
                  return;
                }
                LogicCall(r->logic(), [r, endpoint, kills, reply]() {
                  auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
                  if (ep == nullptr)
                  {
                    reply(CreateJSONError("no endpoint with name " + endpoint));
                    return;
                  }
                  std::size_t removed = 0;
                  for (auto kill : kills)
                  {
                    removed += ep->RemoveAllConvoTagsFor(std::move(kill));
                  }
                  reply(CreateJSONResponse(
                      "removed " + std::to_string(removed) + " flow" + (removed == 1 ? "" : "s")));
                });
              });
            })
        .add_request_command(
            "exit",
            [&](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (r->IsServiceNode())
                {
                  reply(CreateJSONError("not supported"));
                  return;
                }
                std::optional<service::Address> exit;
                std::optional<std::string> lnsExit;
                IPRange range;
                bool map = true;
                const auto exit_itr = obj.find("exit");
                if (exit_itr != obj.end())
                {
                  service::Address addr;
                  const auto exit_str = exit_itr->get<std::string>();
                  if (service::NameIsValid(exit_str))
                  {
                    lnsExit = exit_str;
                  }
                  else if (not addr.FromString(exit_str))
                  {
                    reply(CreateJSONError("invalid exit address"));
                    return;
                  }
                  else
                  {
                    exit = addr;
                  }
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
                LogicCall(
                    r->logic(), [map, exit, lnsExit, range, token, endpoint, r, reply]() mutable {
                      auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
                      if (ep == nullptr)
                      {
                        reply(CreateJSONError("no endpoint with name " + endpoint));
                        return;
                      }
                      if (map and (exit.has_value() or lnsExit.has_value()))
                      {
                        auto mapExit = [=](service::Address addr) mutable {
                          ep->MapExitRange(range, addr);
                          if (token.has_value())
                          {
                            ep->SetAuthInfoForEndpoint(*exit, service::AuthInfo{*token});
                          }
                          ep->EnsurePathToService(
                              addr,
                              [reply, ep, r](auto, service::OutboundContext* ctx) {
                                if (ctx == nullptr)
                                {
                                  reply(CreateJSONError("could not find exit"));
                                  return;
                                }
                                r->routePoker().Up();
                                reply(CreateJSONResponse("OK"));
                              },
                              5s);
                        };
                        if (exit.has_value())
                        {
                          mapExit(*exit);
                        }
                        else if (lnsExit.has_value())
                        {
                          ep->LookupNameAsync(*lnsExit, [reply, mapExit](auto maybe) mutable {
                            if (not maybe.has_value())
                            {
                              reply(CreateJSONError("we could not find an exit with that name"));
                              return;
                            }
                            if (maybe->IsZero())
                            {
                              reply(CreateJSONError("lokinet exit does not exist"));
                              return;
                            }
                            mapExit(*maybe);
                          });
                        }
                        else
                        {
                          reply(
                              CreateJSONError("WTF inconsistent request, no exit address or lns "
                                              "name provided?"));
                        }
                        return;
                      }
                      else if (map and not exit.has_value())
                      {
                        reply(CreateJSONError("no exit address provided"));
                        return;
                      }
                      else if (not map)
                      {
                        r->routePoker().Down();
                        ep->UnmapExitRange(range);
                      }
                      reply(CreateJSONResponse("OK"));
                    });
              });
            })
        .add_request_command("config", [&](oxenmq::Message& msg) {
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

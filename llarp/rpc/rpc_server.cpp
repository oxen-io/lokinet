#include "rpc_server.hpp"
#include <llarp/router/route_poker.hpp>
#include <llarp/constants/version.hpp>
#include <nlohmann/json.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/net/route.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/service/context.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/auth.hpp>
#include <llarp/service/name.hpp>
#include <llarp/router/abstractrouter.hpp>

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

  std::shared_ptr<EndpointBase>
  GetEndpointByName(AbstractRouter* r, std::string name)
  {
    if (r->IsServiceNode())
    {
      return r->exitContext().GetExitEndpoint(name);
    }
    else
    {
      return r->hiddenServiceContext().GetEndpointByName(name);
    }
  }

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
              util::StatusObject result{
                  {"version", llarp::VERSION_FULL}, {"uptime", to_json(r->Uptime())}};
              msg.send_reply(CreateJSONResponse(result));
            })
        .add_request_command(
            "status",
            [&](oxenmq::Message& msg) {
              m_Router->loop()->call([defer = msg.send_later(), r = m_Router]() {
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
            "quic_connect",
            [&](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                std::string endpoint = "default";
                if (auto itr = obj.find("endpoint"); itr != obj.end())
                  endpoint = itr->get<std::string>();

                std::string remoteHost;
                if (auto itr = obj.find("host"); itr != obj.end())
                  remoteHost = itr->get<std::string>();

                uint16_t port = 0;
                if (auto itr = obj.find("port"); itr != obj.end())
                  port = itr->get<uint16_t>();

                int closeID = 0;
                if (auto itr = obj.find("close"); itr != obj.end())
                  closeID = itr->get<int>();

                if (port == 0 and closeID == 0)
                {
                  reply(CreateJSONError("port not provided"));
                  return;
                }
                if (remoteHost.empty() and closeID == 0)
                {
                  reply(CreateJSONError("host not provided"));
                  return;
                }

                r->loop()->call([reply, endpoint, r, remoteHost, port, closeID]() {
                  auto ep = GetEndpointByName(r, endpoint);
                  if (not ep)
                  {
                    reply(CreateJSONError("no such local endpoint"));
                    return;
                  }
                  auto quic = ep->GetQUICTunnel();
                  if (not quic)
                  {
                    reply(CreateJSONError("local endpoint has no quic tunnel"));
                    return;
                  }
                  if (closeID)
                  {
                    quic->close(closeID);
                    reply(CreateJSONResponse("OK"));
                    return;
                  }

                  auto status = std::make_shared<util::StatusObject>();

                  auto hook = [status, reply](bool success) {
                    if (success)
                    {
                      reply(CreateJSONResponse(*status));
                    }
                    else
                    {
                      reply(CreateJSONError("failed"));
                    }
                  };
                  auto [addr, id] = quic->open(remoteHost, port, hook);
                  status->operator[]("addr") = addr.toString();
                  status->operator[]("id") = id;
                });
              });
            })
        .add_request_command(
            "quic_listener",
            [&](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                std::string endpoint = "default";
                if (auto itr = obj.find("endpoint"); itr != obj.end())
                  endpoint = itr->get<std::string>();

                uint16_t port = 0;
                if (auto itr = obj.find("port"); itr != obj.end())
                  port = itr->get<uint16_t>();

                int closeID = 0;
                if (auto itr = obj.find("close"); itr != obj.end())
                  closeID = itr->get<int>();

                if (port == 0 and closeID == 0)
                {
                  reply(CreateJSONError("invalid arguments"));
                  return;
                }
                r->loop()->call([reply, endpoint, port, r, closeID]() {
                  auto ep = GetEndpointByName(r, endpoint);
                  if (not ep)
                  {
                    reply(CreateJSONError("no such local endpoint"));
                    return;
                  }
                  auto quic = ep->GetQUICTunnel();
                  if (not quic)
                  {
                    reply(CreateJSONError("no quic interface available on endpoint " + endpoint));
                    return;
                  }
                  if (port)
                  {
                    int id = 0;
                    try
                    {
                      id = quic->listen(port);
                    }
                    catch (std::exception& ex)
                    {
                      reply(CreateJSONError(ex.what()));
                      return;
                    }
                    util::StatusObject result{{"id", id}};
                    reply(CreateJSONResponse(result));
                  }
                  else if (closeID)
                  {
                    quic->forget(closeID);
                    reply(CreateJSONResponse("OK"));
                  }
                });
              });
            })
        .add_request_command(
            "lookup_snode",
            [&](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (not r->IsServiceNode())
                {
                  reply(CreateJSONError("not supported"));
                  return;
                }
                RouterID routerID;
                if (auto itr = obj.find("snode"); itr != obj.end())
                {
                  std::string remote = itr->get<std::string>();
                  if (not routerID.FromString(remote))
                  {
                    reply(CreateJSONError("invalid remote: " + remote));
                    return;
                  }
                }
                else
                {
                  reply(CreateJSONError("no remote provided"));
                  return;
                }
                std::string endpoint = "default";
                r->loop()->call([r, endpoint, routerID, reply]() {
                  auto ep = r->exitContext().GetExitEndpoint(endpoint);
                  if (ep == nullptr)
                  {
                    reply(CreateJSONError("cannot find local endpoint: " + endpoint));
                    return;
                  }
                  ep->ObtainSNodeSession(routerID, [routerID, ep, reply](auto session) {
                    if (session and session->IsReady())
                    {
                      const auto ip = net::TruncateV6(ep->GetIPForIdent(PubKey{routerID}));
                      util::StatusObject status{{"ip", ip.ToString()}};
                      reply(CreateJSONResponse(status));
                    }
                    else
                      reply(CreateJSONError("failed to obtain snode session"));
                  });
                });
              });
            })
        .add_request_command(
            "endpoint",
            [&](oxenmq::Message& msg) {
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
                r->loop()->call([r, endpoint, kills, reply]() {
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
                r->loop()->call([map, exit, lnsExit, range, token, endpoint, r, reply]() mutable {
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
                      bool shouldSendAuth = false;
                      if (token.has_value())
                      {
                        shouldSendAuth = true;
                        ep->SetAuthInfoForEndpoint(*exit, service::AuthInfo{*token});
                      }
                      ep->EnsurePathToService(
                          addr,
                          [reply, r, shouldSendAuth](auto, service::OutboundContext* ctx) {
                            if (ctx == nullptr)
                            {
                              reply(CreateJSONError("could not find exit"));
                              return;
                            }
                            auto onGoodResult = [r, reply](std::string reason) {
                              r->routePoker().Enable();
                              r->routePoker().Up();
                              reply(CreateJSONResponse(reason));
                            };
                            if (not shouldSendAuth)
                            {
                              onGoodResult("OK");
                              return;
                            }
                            ctx->AsyncSendAuth([onGoodResult, reply](service::AuthResult result) {
                              // TODO: refactor this code.  We are 5 lambdas deep here!
                              if (result.code != service::AuthResultCode::eAuthAccepted)
                              {
                                reply(CreateJSONError(result.reason));
                                return;
                              }
                              onGoodResult(result.reason);
                            });
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
                        if (auto ptr = std::get_if<service::Address>(&*maybe))
                        {
                          mapExit(*ptr);
                        }
                        else
                        {
                          reply(CreateJSONError("lns name resolved to a snode"));
                          return;
                        }
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

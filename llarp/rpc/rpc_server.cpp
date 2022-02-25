#include "rpc_server.hpp"
#include <llarp/router/route_poker.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/util/json.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/service/context.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/auth.hpp>
#include <llarp/service/name.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/dns/dns.hpp>

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
  JSONResponse(Result_t result)
  {
    const auto obj = nlohmann::json{
        {"error", nullptr},
        {"result", result},
    };
    return obj.dump();
  }

  std::string
  JSONError(std::string_view msg)
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
    auto reply = [defer = msg.send_later()](std::string data) { defer.reply(data); };
    const auto maybe = MaybeParseJSON(msg);
    if (not maybe)
    {
      reply(JSONError("failed to parse json"));
      return;
    }
    if (not maybe->is_object())
    {
      reply(JSONError("request data not a json object"));
      return;
    }
    try
    {
      handleRequest(*maybe, reply);
    }
    catch (std::exception& ex)
    {
      reply(JSONError(ex.what()));
    }
  }

  void
  HandleJSONCommand(
      oxenmq::Message& msg,
      std::string replyCmd,
      std::function<void(nlohmann::json, ReplyFunction_t)> handleRequest)
  {
    auto reply = [defer = msg.send_later(), cmd = replyCmd](std::string data) { defer(cmd, data); };
    const auto maybe = MaybeParseJSON(msg);
    if (not maybe)
    {
      reply(JSONError("failed to parse json"));
      return;
    }
    if (not maybe->is_object())
    {
      reply(JSONError("command data not a json object"));
      return;
    }
    try
    {
      handleRequest(*maybe, reply);
    }
    catch (std::exception& ex)
    {
      reply(JSONError(ex.what()));
    }
  }

  void
  GetExit(AbstractRouter* r, nlohmann::json obj, ReplyFunction_t reply)
  {
    if (r->IsServiceNode())
    {
      reply(JSONError("not supported"));
      return;
    }
    std::optional<llarp::service::Address> exit;
    std::optional<std::string> lnsExit;
    IPRange range;
    bool map = true;

    if (auto maybe = llarp::json::maybe_get<bool>(obj, "unmap", false))
    {
      map = *maybe != true;
    }

    if (auto maybe_exit = llarp::json::maybe_get<std::string_view>(obj, "exit"))
    {
      auto exit_str = TrimWhitespace(*maybe_exit);
      service::Address addr{};
      if (service::NameIsValid(exit_str))
      {
        lnsExit = exit_str;
      }
      else if (exit_str == "null" or addr.FromString(exit_str))
      {
        exit = addr;
      }
      else
        throw std::runtime_error{"invalid exit address"};
    }
    else if (map)
      throw std::runtime_error{"no exit provided"};

    if (auto maybe_range = json::maybe_get<std::string>(obj, "range", "::/0"))
    {
      auto range_str = *maybe_range;
      if (not range.FromString(range_str))
        throw std::runtime_error{"invalid range"};
    }

    auto token = json::maybe_get<std::string>(obj, "token");

    std::string endpoint;

    if (auto maybe_endpoint = json::maybe_get<std::string>(obj, "endpoint", "default"))
      endpoint = *maybe_endpoint;

    auto accessLogic = [map, exit, lnsExit, range, token, endpoint, r, reply]() {
      auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
      if (ep == nullptr)
      {
        reply(JSONError("no endpoint with name " + endpoint));
        return;
      }
      if (not map)
      {
        r->routePoker().Down();
        ep->UnmapExitRange(range);
        reply(JSONResponse("OK"));
        return;
      }

      auto onAuthResult = [r, reply, ep, range](service::AuthResult result) {
        if (result.code == service::AuthResultCode::eAuthAccepted)
        {
          if (r->HasClientExit())
            reply(JSONResponse(result.reason));
          else
            reply(JSONError("we dont have an exit?"));
        }
        else
        {
          r->routePoker().Down();
          ep->UnmapExitRange(range);
          reply(JSONError(result.reason));
        }
      };

      auto mapExit = [onAuthResult, r, ep, range, token, exit](service::Address addr) {
        ep->MapExitRange(range, addr);
        r->routePoker().Enable();
        r->routePoker().Up();
        bool shouldSendAuth = false;
        if (token)
        {
          shouldSendAuth = true;
          ep->SetAuthInfoForEndpoint(*exit, service::AuthInfo{*token});
        }

        if (addr.IsZero())
        {
          onAuthResult(
              service::AuthResult{service::AuthResultCode::eAuthAccepted, "OK: added null exit"});
          return;
        }
        ep->MarkAddressOutbound(addr);
        ep->EnsurePathToService(
            addr, [shouldSendAuth, onAuthResult](auto, service::OutboundContext* ctx) {
              if (ctx == nullptr)
              {
                onAuthResult(service::AuthResult{
                    service::AuthResultCode::eAuthFailed, "could not find exit"});
                return;
              }
              if (not shouldSendAuth)
              {
                onAuthResult(
                    service::AuthResult{service::AuthResultCode::eAuthAccepted, "OK: connected"});
                return;
              }
              ctx->AsyncSendAuth(onAuthResult);
            });
      };
      if (exit)
      {
        mapExit(*exit);
        return;
      }
      ep->LookupNameAsync(*lnsExit, [reply, mapExit](auto maybe) {
        if (not maybe)
        {
          reply(JSONError("we could not find an exit with that name"));
          return;
        }
        if (auto ptr = std::get_if<service::Address>(&*maybe))
        {
          if (ptr->IsZero())
            reply(JSONError("name does not exist"));
          else
            mapExit(*ptr);
          return;
        }
        reply(JSONError("lns name resolved to a snode"));
      });
    };
    r->loop()->call(accessLogic);
  }

  void
  RpcServer::AsyncServeRPC(oxenmq::address url)
  {
    m_LMQ->listen_plain(url.zmq_address());
    m_LMQ->add_category("llarp", oxenmq::AuthLevel::none)
        .add_command(
            "halt",
            [this](oxenmq::Message& msg) {
              if (not m_Router->IsRunning())
              {
                msg.send_reply(JSONError("router is not running"));
                return;
              }
              msg.send_reply(JSONResponse("OK"));
              m_Router->Stop();
            })
        .add_request_command(
            "version",
            [r = m_Router](oxenmq::Message& msg) {
              util::StatusObject result{
                  {"version", llarp::VERSION_FULL}, {"uptime", to_json(r->Uptime())}};
              msg.send_reply(JSONResponse(result));
            })
        .add_request_command(
            "status",
            [this](oxenmq::Message& msg) {
              m_Router->loop()->call([defer = msg.send_later(), r = m_Router]() {
                std::string data;
                if (r->IsRunning())
                {
                  data = JSONResponse(r->ExtractStatus());
                }
                else
                {
                  data = JSONError("router not yet ready");
                }
                defer.reply(data);
              });
            })
        .add_request_command(
            "get_status",
            [this](oxenmq::Message& msg) {
              m_Router->loop()->call([defer = msg.send_later(), r = m_Router]() {
                defer.reply(JSONResponse(r->ExtractSummaryStatus()));
              });
            })
        .add_request_command(
            "quic_connect",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                std::string endpoint;
                if (auto maybe = json::maybe_get<std::string>(obj, "endpoint", "default"))
                  endpoint = *maybe;

                std::string bindAddr;
                if (auto maybe = json::maybe_get<std::string>(obj, "bind", "127.0.0.1:0"))
                  bindAddr = *maybe;

                std::string remoteHost;
                if (auto maybe = json::maybe_get<std::string>(obj, "host", ""))
                  remoteHost = *maybe;

                uint16_t port = 0;
                if (auto maybe = json::maybe_get<uint16_t>(obj, "port", 0))
                  port = *maybe;

                int closeID = 0;
                if (auto maybe = json::maybe_get<int>(obj, "close", 0))
                  closeID = *maybe;

                if (port == 0 and closeID == 0)
                  throw std::runtime_error{"port not provided"};

                if (remoteHost.empty() and closeID == 0)
                  throw std::runtime_error{"host not provided"};

                SockAddr laddr{};
                laddr.fromString(bindAddr);

                r->loop()->call([reply, endpoint, r, remoteHost, port, closeID, laddr]() {
                  auto ep = GetEndpointByName(r, endpoint);
                  if (not ep)
                  {
                    reply(JSONError("no such local endpoint"));
                    return;
                  }
                  auto quic = ep->GetQUICTunnel();
                  if (not quic)
                  {
                    reply(JSONError("local endpoint has no quic tunnel"));
                    return;
                  }
                  if (closeID)
                  {
                    quic->close(closeID);
                    reply(JSONResponse("OK"));
                    return;
                  }

                  try
                  {
                    auto [addr, id] = quic->open(
                        remoteHost, port, [](auto&&) {}, laddr);
                    util::StatusObject status;
                    status["addr"] = addr.toString();
                    status["id"] = id;
                    reply(JSONResponse(status));
                  }
                  catch (std::exception& ex)
                  {
                    reply(JSONError(ex.what()));
                  }
                });
              });
            })
        .add_request_command(
            "quic_listener",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                std::string endpoint{};
                if (auto maybe = json::maybe_get<std::string>(obj, "endpoint", "default"))
                  endpoint = *maybe;

                std::string remote{};
                if (auto maybe = json::maybe_get<std::string>(obj, "host", "127.0.0.1"))
                  remote = *maybe;

                uint16_t port{};
                if (auto maybe = json::maybe_get<uint16_t>(obj, "port", 0))
                  port = *maybe;

                int closeID{};
                if (auto maybe = json::maybe_get<int>(obj, "close", 0))
                  closeID = *maybe;

                std::string srvProto{};
                if (auto maybe = json::maybe_get<std::string>(obj, "srv-proto", ""))
                  srvProto = *maybe;

                if (port == 0 and closeID == 0)
                  throw std::runtime_error{"invalid arguments"};

                r->loop()->call([reply, endpoint, remote, port, r, closeID, srvProto]() {
                  auto ep = GetEndpointByName(r, endpoint);
                  if (not ep)
                  {
                    reply(JSONError("no such local endpoint"));
                    return;
                  }
                  auto quic = ep->GetQUICTunnel();
                  if (not quic)
                  {
                    reply(JSONError("no quic interface available on endpoint " + endpoint));
                    return;
                  }
                  if (port)
                  {
                    int id = 0;
                    try
                    {
                      SockAddr addr{remote + ":" + std::to_string(port)};
                      id = quic->listen(addr);
                    }
                    catch (std::exception& ex)
                    {
                      reply(JSONError(ex.what()));
                      return;
                    }
                    util::StatusObject result;
                    result["id"] = id;
                    std::string localAddress =
                        var::visit([](auto&& addr) { return addr.ToString(); }, ep->LocalAddress());
                    result["addr"] = localAddress + ":" + std::to_string(port);
                    if (not srvProto.empty())
                    {
                      auto srvData =
                          dns::SRVData::fromTuple(std::make_tuple(srvProto, 1, 1, port, ""));
                      ep->PutSRVRecord(std::move(srvData));
                    }
                    reply(JSONResponse(result));
                  }
                  else if (closeID)
                  {
                    quic->forget(closeID);
                    reply(JSONResponse("OK"));
                  }
                });
              });
            })
        .add_request_command(
            "lookup_snode",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (not r->IsServiceNode())
                  throw std::runtime_error{"not supported"};

                RouterID routerID;
                if (auto maybe = json::maybe_get<std::string>(obj, "snode"))
                {
                  if (not routerID.FromString(*maybe))
                    throw std::runtime_error{"invalid remote: " + *maybe};
                }
                else
                  throw std::runtime_error{"no remote provided"};

                std::string endpoint = "default";
                r->loop()->call([r, endpoint, routerID, reply]() {
                  auto ep = r->exitContext().GetExitEndpoint(endpoint);
                  if (ep == nullptr)
                  {
                    reply(JSONError("cannot find local endpoint: " + endpoint));
                    return;
                  }
                  ep->ObtainSNodeSession(routerID, [routerID, ep, reply](auto session) {
                    if (session and session->IsReady())
                    {
                      const auto ip = net::TruncateV6(ep->GetIPForIdent(PubKey{routerID}));
                      util::StatusObject status{{"ip", ip.ToString()}};
                      reply(JSONResponse(status));
                    }
                    else
                      reply(JSONError("failed to obtain snode session"));
                  });
                });
              });
            })
        .add_request_command(
            "endpoint",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                if (r->IsServiceNode())
                  throw std::runtime_error{"not supported"};

                std::string endpoint;
                std::unordered_set<service::Address> kills;
                if (auto maybe = json::maybe_get<std::string>(obj, "endpoint", "default"))
                  endpoint = *maybe;

                if (auto maybe = json::maybe_get<nlohmann::json>(obj, "kills"))
                {
                  if (maybe->is_array())
                  {
                    for (auto itr = maybe->begin(); itr != maybe->end(); ++itr)
                    {
                      if (itr->is_string())
                        kills.emplace(itr->get<std::string>());
                    }
                  }
                  else if (maybe->is_string())
                  {
                    kills.emplace(maybe->get<std::string>());
                  }
                }
                if (kills.empty())
                  throw std::runtime_error{"no action taken"};

                r->loop()->call([r, endpoint, kills, reply]() {
                  auto ep = r->hiddenServiceContext().GetEndpointByName(endpoint);
                  if (ep == nullptr)
                  {
                    reply(JSONError("no endpoint with name " + endpoint));
                    return;
                  }
                  std::size_t removed = 0;
                  for (auto kill : kills)
                  {
                    removed += ep->RemoveAllConvoTagsFor(std::move(kill));
                  }
                  reply(JSONResponse(
                      "removed " + std::to_string(removed) + " flow" + (removed == 1 ? "" : "s")));
                });
              });
            })
        .add_command(
            "async-exit",
            [this](oxenmq::Message& msg) {
              HandleJSONCommand(
                  msg, "inform-exit", [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                    GetExit(r, std::move(obj), std::move(reply));
                  });
            })
        .add_request_command(
            "exit",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                GetExit(r, std::move(obj), std::move(reply));
              });
            })
        .add_request_command(
            "dns_query",
            [this](oxenmq::Message& msg) {
              HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
                std::string endpoint{};
                if (auto maybe = json::maybe_get<std::string>(obj, "endpoint", "default"))
                  endpoint = *maybe;

                std::string qname{};
                dns::QType_t qtype{};
                if (auto maybe = json::maybe_get<std::string>(obj, "qname"))
                  qname = *maybe;
                else
                  throw std::runtime_error{"no qname provided"};

                if (auto maybe = json::maybe_get<dns::QType_t>(obj, "qtype", dns::qTypeA))
                  qtype = *maybe;

                dns::Message msg{dns::Question{qname, qtype}};
                // TODO: race condition
                if (auto ep_ptr = (GetEndpointByName(r, endpoint)))
                {
                  if (auto ep = reinterpret_cast<dns::IQueryHandler*>(ep_ptr.get()))
                  {
                    if (ep->ShouldHookDNSMessage(msg))
                    {
                      ep->HandleHookedDNSMessage(std::move(msg), [reply](dns::Message msg) {
                        reply(JSONResponse(msg.ToJSON()));
                      });
                      return;
                    }
                  }
                  throw std::runtime_error{"dns query not accepted by endpoint"};
                }
                throw std::runtime_error{"no such endpoint for dns query"};
              });
            })
        .add_request_command("config", [this](oxenmq::Message& msg) {
          HandleJSONRequest(msg, [r = m_Router](nlohmann::json obj, ReplyFunction_t reply) {
            if (auto maybe = json::maybe_get<nlohmann::json>(obj, "override"))
            {
              if (not maybe->is_object())
                throw std::runtime_error{"override is not an object"};

              for (const auto& [section, value] : maybe->items())
              {
                if (not value.is_object())
                  throw std::runtime_error{
                      stringify("failed to set [", section, "] section is not an object")};

                for (const auto& [key, value] : value.items())
                {
                  if (not value.is_string())
                    throw std::runtime_error{
                        stringify("failed to set [", section, "]:", key, " value is not a string")};
                  // TODO: race condition
                  r->GetConfig()->Override(section, key, value.get<std::string>());
                }
              }
            }

            if (auto maybe = json::maybe_get<bool>(obj, "reload", false); *maybe)
            {
              r->QueueDiskIO([conf = r->GetConfig()]() { conf->Save(); });
            }
            reply(JSONResponse("OK"));
          });
        });
  }
}  // namespace llarp::rpc

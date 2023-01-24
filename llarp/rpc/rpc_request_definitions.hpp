#pragma once

#include "rpc_request_decorators.hpp"
#include "llarp/net/ip_range.hpp"
#include "llarp/router/abstractrouter.hpp"
#include "llarp/router/route_poker.hpp"
#include "llarp/service/address.hpp"
#include "llarp/service/endpoint.hpp"
#include "llarp/service/outbound_context.hpp"
#include <string_view>
#include <llarp/config/config.hpp>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <oxen/log/omq_logger.hpp>
#include <unordered_map>

namespace llarp::rpc
{
  //  RPC: halt
  //    Stops lokinet router
  //
  //  Inputs: none
  //
  struct Halt : NoArgs, Immediate
  {
    static constexpr auto name = "halt"sv;
  };

  //  RPC: version
  //    Returns version and uptime information
  //
  //  Inputs: none
  //
  //  Returns: "OK"
  //    "uptime"
  //    "version"
  //
  struct Version : NoArgs, Immediate
  {
    static constexpr auto name = "version"sv;
  };

  //  RPC: status
  //    Returns that current activity status of lokinet router
  //    Calls router::extractstatus
  //
  //  Inputs: none
  //
  //  Returns: massive dump of status info including
  //    "running"
  //    "numNodesKnown"
  //    "dht"
  //    "services"
  //    "exit"
  //    "links"
  //    "outboundMessages"
  //    etc
  //
  struct Status : NoArgs
  {
    static constexpr auto name = "status"sv;
  };

  //  RPC: get_status
  //    Returns current summary status
  //
  //  Inputs: none
  //
  //  Returns: slightly smaller dump of status info including
  //    "authcodes"
  //    "exitMap"
  //    "lokiAddress"
  //    "networkReady"
  //    "numPathsBuilt"
  //    "numPeersConnected"
  //    etc
  //
  struct GetStatus : NoArgs
  {
    static constexpr auto name = "get_status"sv;
  };

  //  RPC: quic_connect
  //    Initializes QUIC connection tunnel
  //    Passes request parameters in nlohmann::json format
  //
  //  Inputs:
  //    "endpoint" : endpoint id (string)
  //    "bindAddr" : bind address (string, ex: "127.0.0.1:1142")
  //    "host" : remote host ID (string)
  //    "port" : port to bind to (int)
  //    "close" : close connection to port or host ID
  //
  //  Returns:
  //    "id" : connection ID
  //    "addr" : connection local address
  //
  struct QuicConnect : RPCRequest
  {
    static constexpr auto name = "quic_connect"sv;

    struct request_parameters
    {
      std::string bindAddr;
      int closeID;
      std::string endpoint;
      uint16_t port;
      std::string remoteHost;
    } request;
  };

  //  RPC: quick_listener
  //    Connects to QUIC interface on local endpoint
  //    Passes request parameters in nlohmann::json format
  //
  //  Inputs:
  //    "endpoint" : endpoint id (string)
  //    "host" : remote host ID (string)
  //    "port" : port to bind to (int)
  //    "close" : close connection to port or host ID
  //    "srv-proto" :
  //
  //  Returns:
  //    "id" : connection ID
  //    "addr" : connection local address
  //
  struct QuicListener : RPCRequest
  {
    static constexpr auto name = "quic_listener"sv;

    struct request_parameters
    {
      int closeID;
      std::string endpoint;
      uint16_t port;
      std::string remoteHost;
      std::string srvProto;
    } request;
  };

  //  RPC: lookup_snode
  //    Look up service node
  //    Passes request parameters in nlohmann::json format
  //
  //  Inputs:
  //    "routerID" : router ID to query (string)
  //
  //  Returns:
  //    "ip" : snode IP address
  //
  struct LookupSnode : RPCRequest
  {
    static constexpr auto name = "lookup_snode"sv;

    struct request_parameters
    {
      std::string routerID;
    } request;
  };

  //  RPC: exit
  //    Seems like this adds an exit node?
  //
  //  Note: ask Jason about the internals of this
  //
  //  Inputs:
  //    "endpoint" :
  //    "unmap" : if true, unmaps connection to exit node (bool)
  //    "range" : IP range to map to exit node
  //    "token" :
  //
  //  Returns:
  //
  struct Exit : RPCRequest
  {
    static constexpr auto name = "exit"sv;

    struct request_parameters
    {
      std::string address;
      std::string ip_range;
      std::string token;
      bool unmap;
    } request;

    void
    onGoodResult(std::string reason, bool hasClient)
    {
      response = (hasClient) ? nlohmann::json{{"result", reason}}.dump()
                             : nlohmann::json{{"error", "We don't have an exit?"}}.dump();
    }

    void
    onBadResult(
        std::string reason, AbstractRouter& abs, llarp::service::Endpoint_ptr eptr, IPRange range)
    {
      abs.routePoker()->Down();
      eptr->UnmapExitRange(range);
      response = nlohmann::json{{"result", reason}}.dump();
    }

    void
    mapExit(
        service::Address addr,
        AbstractRouter& router,
        llarp::service::Endpoint_ptr eptr,
        IPRange range,
        service::Address exitAddr)
    {
      eptr->MapExitRange(range, addr);

      bool sendAuth = (request.token.empty()) ? false : true;
      if (sendAuth)
        eptr->SetAuthInfoForEndpoint(exitAddr, service::AuthInfo{request.token});

      if (addr.IsZero())
      {
        onGoodResult("Null exit added", router.HasClientExit());
        return;
      }

      eptr->MarkAddressOutbound(addr);

      eptr->EnsurePathToService(addr, [&](auto, service::OutboundContext* ctx) {
        if (ctx == nullptr)
        {
          onBadResult("Could not find exit", router, eptr, range);
          return;
        }
        if (not sendAuth)
        {
          onGoodResult("OK: connected to " + addr.ToString(), router.HasClientExit());
          return;
        }
        //  only lambda that we will keep
        ctx->AsyncSendAuth([&](service::AuthResult result) {
          if (result.code != service::AuthResultCode::eAuthAccepted)
          {
            onBadResult(result.reason, router, eptr, range);
            return;
          }
          onGoodResult(result.reason, router.HasClientExit());
          return;
        });
      });
    }
  };

  //  RPC: dns_query
  //    Attempts to query endpoint by domain name
  //
  //  Note: ask Jason about the internals of this
  //
  //  Inputs:
  //    "endpoint" : endpoint ID to query (string)
  //    "qname" : query name (string)
  //    "qtype" : query type (int)
  //
  //  Returns:
  //
  struct DNSQuery : Immediate
  {
    static constexpr auto name = "dns_query"sv;

    struct request_parameters
    {
      std::string endpoint;
      uint16_t qtype;
      std::string qname;
    } request;
  };

  //  RPC: config
  //    Runs lokinet router using .ini config file passed as path
  //
  //  Inputs:
  //    "filename" : name of .ini file to either save or delete
  //    "ini" : .ini chunk to save in new file
  //    "del" : boolean specifying whether to delete file "filename" or save it
  //
  //  Returns:
  //
  struct Config : Immediate
  {
    static constexpr auto name = "config"sv;

    struct request_parameters
    {
      bool del;
      std::string filename;
      std::string ini;
    } request;
  };

  // List of all RPC request structs to allow compile-time enumeration of all supported types
  using rpc_request_types = tools::type_list<
      Halt,
      Version,
      Status,
      GetStatus,
      QuicConnect,
      QuicListener,
      LookupSnode,
      Exit,
      DNSQuery,
      Config>;

}  // namespace llarp::rpc

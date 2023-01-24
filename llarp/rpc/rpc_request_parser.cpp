#include "rpc_request_parser.hpp"
#include "llarp/rpc/rpc_request_definitions.hpp"
#include "param_parser.hpp"
#include <string_view>
#include <llarp/config/config.hpp>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <oxen/log/omq_logger.hpp>

namespace llarp::rpc
{
  using nlohmann::json;

  void
  parse_request(QuicConnect& quicconnect, rpc_input input)
  {
    get_values(
        input,
        "bindAddr",
        quicconnect.request.bindAddr,
        "closeID",
        quicconnect.request.closeID,
        "endpoint",
        quicconnect.request.endpoint,
        "port",
        quicconnect.request.port,
        "remoteHost",
        quicconnect.request.remoteHost);
  }

  void
  parse_request(QuicListener& quiclistener, rpc_input input)
  {
    get_values(
        input,
        "closeID",
        quiclistener.request.closeID,
        "endpoint",
        quiclistener.request.endpoint,
        "port",
        quiclistener.request.port,
        "remoteHost",
        quiclistener.request.remoteHost,
        "srvProto",
        quiclistener.request.srvProto);
  }

  void
  parse_request(LookupSnode& lookupsnode, rpc_input input)
  {
    get_values(input, "routerID", lookupsnode.request.routerID);
  }

  void
  parse_request(MapExit& mapexit, rpc_input input)
  {
    get_values(
        input,
        "address",
        mapexit.request.address,
        "IP_range",
        mapexit.request.ip_range,
        "token",
        mapexit.request.token);
  }

  void
  parse_request(UnmapExit& unmapexit, rpc_input input)
  {
    get_values(
        input,
        "IP_range",
        unmapexit.request.ip_range);
  }

  void
  parse_request(DNSQuery& dnsquery, rpc_input input)
  {
    get_values(
        input,
        "endpoint",
        dnsquery.request.endpoint,
        "qname",
        dnsquery.request.qname,
        "qtype",
        dnsquery.request.qtype);
  }

  void
  parse_request(Config& config, rpc_input input)
  {
    get_values(
        input,
        "delete",
        config.request.del,
        "filename",
        config.request.filename,
        "ini",
        config.request.ini);
  }

}  // namespace llarp::rpc
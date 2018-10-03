#ifndef LIBLLARP_DNS_DOTLOKILOOKUP_HPP
#define LIBLLARP_DNS_DOTLOKILOOKUP_HPP

#include <llarp/service/address.hpp>
//#include <llarp/service.hpp>
//#include <llarp/service/endpoint.hpp>
//#include <llarp/handlers/tun.hpp>
//#include <llarp/handlers/tun.hpp>

#include "dnsd.hpp"

typedef bool (*map_address_hook_func)(const llarp::service::Address &addr,
                                      uint32_t ip);

/// dotLokiLookup context/config
struct dotLokiLookup
{
  /// for timers (MAYBEFIXME? maybe we decouple this, yes pls have a generic
  /// passed in)
  struct llarp_logic *logic;
  /// which ip tracker to use
  struct dns_iptracker *ip_tracker;

  /// tunEndpoint
  // llarp::handlers::TunEndpoint *tunEndpoint; // is this even needed here?
  void *user;  // well dotLokiLookup current uses it to access the tun if
  // pointer to tunendpoint properties?
  // llarp::service::Context *hiddenServiceContext;

  // need a way to reference
  // 1. mapaddress
  map_address_hook_func map_address_handler;
  // std::function< bool(const llarp::service::Address &addr, uint32_t ip),
  // llarp::handlers::TunEndpoint * > callback;
  // 2. prefetch
};

dnsd_query_hook_response *
llarp_dotlokilookup_handler(std::string name, const struct sockaddr *from,
                            struct dnsd_question_request *const request);

#endif

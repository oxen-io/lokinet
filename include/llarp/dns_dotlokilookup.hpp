#ifndef LIBLLARP_DNS_DOTLOKILOOKUP_HPP
#define LIBLLARP_DNS_DOTLOKILOOKUP_HPP

#include <llarp/service/address.hpp>

#include "dnsd.hpp"

struct dotLokiLookup;

using obtain_address_func =
    std::function< bool(struct dotLokiLookup *, const byte_t *addr,
                        bool isSNode, llarp::huint32_t &ip) >;

/// dotLokiLookup context/config
struct dotLokiLookup
{
  struct dns_iptracker *ip_tracker;
  /// opaque user data
  void *user;
  /// get address for lookup
  obtain_address_func obtainAddress;
};

void
llarp_dotlokilookup_handler(std::string name,
                            const dnsd_question_request *request,
                            struct dnsd_query_hook_response *result);

#endif

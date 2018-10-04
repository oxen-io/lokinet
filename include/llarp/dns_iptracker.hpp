#ifndef LIBLLARP_DNSIPTRACKER_HPP
#define LIBLLARP_DNSIPTRACKER_HPP

#include <llarp/net.hpp>
#include <map>
#include <vector>
#include <llarp/service/address.hpp>
#include "dns_dotlokilookup.hpp"

// either a request or response?
// neither, it's a result set row
struct dns_pointer
{
  llarp::Addr hostResult;
  llarp::service::Address b32addr;
  // we could store the timeout at which we expect it to be available
  // or a list of pending requests for it
};

struct ip_range
{
  uint8_t octet2;
  uint8_t octet3;
  uint8_t left;
  std::unordered_map< uint8_t, dns_pointer * > used;
};

struct dns_iptracker
{
  struct privatesInUse interfaces;
  struct privatesInUse used_privates;
  std::vector< std::unique_ptr< ip_range > > used_ten_ips;
  std::vector< std::unique_ptr< ip_range > > used_seven_ips;
  std::vector< std::unique_ptr< ip_range > > used_nine_ips;
};

void
dns_iptracker_init();

bool
dns_iptracker_setup_dotLokiLookup(dotLokiLookup *dll, llarp::Addr tunGatewayIp);

bool
dns_iptracker_setup(dns_iptracker *iptracker, llarp::Addr tunGatewayIp);

struct dns_pointer *
dns_iptracker_get_free();

struct dns_pointer *
dns_iptracker_get_free(dns_iptracker *iptracker);

#endif

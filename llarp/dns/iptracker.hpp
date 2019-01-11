#ifndef LLARP_DNSIPTRACKER_HPP
#define LLARP_DNSIPTRACKER_HPP

#include <dns/dotlokilookup.hpp>
#include <net/net.hpp>
#include <service/address.hpp>

#include <map>
#include <vector>

// either a request or response?
// neither, it's a result set row
struct dns_pointer
{
  llarp::huint32_t hostResult;
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
  // make it easier to find a entry
  std::vector< std::unique_ptr< dns_pointer > > map;
};

void
dns_iptracker_init();

bool
dns_iptracker_setup_dotLokiLookup(dotLokiLookup *dll,
                                  llarp::huint32_t tunGatewayIp);

bool
dns_iptracker_setup(dns_iptracker *iptracker, llarp::huint32_t tunGatewayIp);

struct dns_pointer *
dns_iptracker_get_free();

struct dns_pointer *
dns_iptracker_get_free(dns_iptracker *iptracker);

#endif

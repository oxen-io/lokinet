#ifndef LIBLLARP_DNSIPTRACKER_HPP
#define LIBLLARP_DNSIPTRACKER_HPP

#include <llarp/net.hpp>
#include <map>
#include <vector>

// either a request or response?
struct dns_pointer
{
  struct sockaddr *hostResult;
};

struct ip_range
{
  uint8_t octet2;
  uint8_t octet3;
  // FIXME: we're not consecutive
  uint8_t left;
  std::map< uint8_t, dns_pointer * > used;
};

struct dns_iptracker
{
  struct privatesInUse interfaces;
  struct privatesInUse used_privates;
  std::vector< ip_range * > used_ten_ips;
  std::vector< ip_range * > used_seven_ips;
  std::vector< ip_range * > used_nine_ips;
};

void
dns_iptracker_init();

struct dns_pointer *
dns_iptracker_get_free();

#endif

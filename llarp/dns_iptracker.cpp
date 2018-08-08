#include "dns_iptracker.hpp"  //
#include "logger.hpp"

dns_iptracker g_dns_iptracker;

void
dns_iptracker_init()
{
  g_dns_iptracker.interfaces    = llarp_getPrivateIfs();
  g_dns_iptracker.used_privates = g_dns_iptracker.interfaces;
}

inline struct dns_pointer *
dns_iptracker_allocate_range(struct ip_range *range)
{
  // we have an IP
  llarp::LogInfo("Range has ", (unsigned int)range->left, " ips left");
  range->left--;  // use it up
  struct dns_pointer *result = new dns_pointer;
  // llarp::Addr ip(10, range->octet2, range->octet3, range->left + 2);
  // llarp::LogInfo("Allocated ", ip);
  result->hostResult = new sockaddr;
  // ip.CopyInto(result->hostResult);
  result->hostResult->sa_family = AF_INET;
  result->hostResult->sa_len    = sizeof(in_addr);
  struct sockaddr_in *ipv4      = (struct sockaddr_in *)result->hostResult;
  struct in_addr *addr          = &ipv4->sin_addr;
  unsigned char *ip2            = (unsigned char *)&(addr->s_addr);
  ip2[0]                        = 10;
  ip2[1]                        = range->octet2;
  ip2[2]                        = range->octet3;
  ip2[3]                        = range->left + 2;
  llarp::Addr test(*result->hostResult);
  llarp::LogInfo("Allocated Test ", test);
  // make an address and place into this sockaddr
  range->used[range->left + 2] = result;
  return result;
}

struct dns_pointer *
dns_iptracker_check_range(std::vector< ip_range * > &ranges)
{
  // tens not all used up
  if(ranges.size())
  {
    // FIXME: maybe find_if where left not 0
    // find a range
    for(auto it = ranges.begin(); it != ranges.end(); ++it)
    {
      if((*it)->left)
      {
        struct dns_pointer *result = dns_iptracker_allocate_range(*it);
        if(!(*it)->left)
        {
          // all used up
          // FIXME: are there any more octets available?
        }
        return result;
      }
    }
  }
  else
  {
    // create one
    auto new_range    = new ip_range;
    new_range->octet2 = 0;
    new_range->octet3 = 0;    // FIXME: counter
    new_range->left   = 252;  // 0 is net, 1 is gw, 255 is broadcast
    ranges.push_back(new_range);
    // don't need to check if we're out since this is fresh range
    return dns_iptracker_allocate_range(new_range);
  }
  return nullptr;
}

struct dns_pointer *
dns_iptracker_get_free()
{
  if(!g_dns_iptracker.used_privates.ten)
  {
    struct dns_pointer *test =
        dns_iptracker_check_range(g_dns_iptracker.used_ten_ips);
    if(test)
    {
      return test;
    }
  }
  if(!g_dns_iptracker.used_privates.oneSeven)
  {
    struct dns_pointer *test =
        dns_iptracker_check_range(g_dns_iptracker.used_seven_ips);
    if(test)
    {
      return test;
    }
  }
  if(!g_dns_iptracker.used_privates.oneNine)
  {
    struct dns_pointer *test =
        dns_iptracker_check_range(g_dns_iptracker.used_nine_ips);
    if(test)
    {
      return test;
    }
  }
  return nullptr;
}

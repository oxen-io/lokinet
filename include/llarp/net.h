#ifndef LLARP_NET_H
#define LLARP_NET_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#ifdef __linux__
#include <linux/if_packet.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
  
bool llarp_getifaddr(const char * ifname, int af, struct sockaddr* addr);
  
#ifdef __cplusplus
}
#endif
#endif

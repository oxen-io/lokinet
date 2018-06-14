#ifndef LLARP_NET_H
#define LLARP_NET_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr);

#ifdef __cplusplus
}
#endif
#endif

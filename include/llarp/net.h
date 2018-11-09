#ifndef LLARP_NET_H
#define LLARP_NET_H
#if defined(_WIN32) || defined(__MINGW32__)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef unsigned short in_port_t;
typedef unsigned int in_addr_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <stdbool.h>
#include <sys/types.h>

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr);

#endif

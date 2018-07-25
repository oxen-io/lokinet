#ifndef LLARP_NET_H
#define LLARP_NET_H
#if defined(__MINGW32__)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr);

#endif

#ifndef LLARP_NET_NET_IF_HPP
#define LLARP_NET_NET_IF_HPP
#ifndef _WIN32
// this file is a shim include for #include <net/if.h>

// Work around for broken glibc/linux header definitions in xenial that makes
// including both net/if.h (which we need for if_nametoindex) and linux/if.h
// (which tuntap.h includes) impossible.  When we stop supporting xenial we can
// remove this mess and just include net/if.h here.
#if defined(Linux) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 23
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) \
    && LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define _NET_IF_H 1
#ifndef IFNAMSIZ
#define IFNAMSIZ (16)
#endif
#include <linux/if.h>
extern "C" unsigned int
if_nametoindex(const char* __ifname) __THROW;
#endif
#else
#include <net/if.h>
#endif
#endif
#endif

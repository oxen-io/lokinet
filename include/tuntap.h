/*
 * Copyright (c) 2012 Tristan Le Guern <leguern AT medu DOT se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * Copyright (c) 2016 Mahdi Mokhtari <mokhi64@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#if defined Windows
#else /* Unix */
#include <sys/socket.h>
#endif

#if !defined Windows /* Unix :) */
#if !defined Linux
#include <netinet/in.h>
#endif

#if defined(Linux)
// Once we drop xenial support we can just include net/if.h on linux
#include <linux/if.h>
#else
#include <net/if.h>
#endif

#if defined Linux
#include <netinet/in.h>
#elif defined(iOS)
#include <net/ethernet.h>
#else
#include <netinet/if_ether.h>
#endif
#endif

#include <stdint.h>

#ifndef LIBTUNTAP_H_
#define LIBTUNTAP_H_

#if defined IFNAMSIZ && !defined IF_NAMESIZE
#define IF_NAMESIZE IFNAMSIZ /* Historical BSD name */
#elif !defined IF_NAMESIZE
#define IF_NAMESIZE 16
#endif

#define IF_DESCRSIZE 50 /* XXX: Tests needed on NetBSD and OpenBSD */

#if defined TUNSETDEBUG
#define TUNSDEBUG TUNSETDEBUG
#endif

#if defined Windows
#define TUNFD_INVALID_VALUE INVALID_HANDLE_VALUE
#else /* Unix */
#define TUNFD_INVALID_VALUE -1
#endif

/*
 * Uniformize types
 * - t_tun: tun device file descriptor
 * - t_tun_in_addr: struct in_addr/IN_ADDR
 * - t_tun_in6_addr: struct in6_addr/IN6_ADDR
 */
#if defined Windows
#include <windows.h>
#include <in6addr.h>
#include <winsock2.h>
typedef HANDLE t_tun;
typedef IN_ADDR t_tun_in_addr;
typedef IN6_ADDR t_tun_in6_addr;
#else /* Unix */
typedef int t_tun;
typedef struct in_addr t_tun_in_addr;
typedef struct in6_addr t_tun_in6_addr;
#endif

/*
 * Windows helpers
 */
#if defined Windows
//#define strncat(x, y, z) strncat_s((x), _countof(x), (y), (z));
#define strdup(x) _strdup(x)
#endif

#define TUNTAP_ID_MAX 256
#define TUNTAP_ID_ANY 257

#define TUNTAP_MODE_ETHERNET 0x0001
#define TUNTAP_MODE_TUNNEL 0x0002
#define TUNTAP_MODE_PERSIST 0x0004

#define TUNTAP_LOG_NONE 0x0000
#define TUNTAP_LOG_DEBUG 0x0001
#define TUNTAP_LOG_INFO 0x0002
#define TUNTAP_LOG_NOTICE 0x0004
#define TUNTAP_LOG_WARN 0x0008
#define TUNTAP_LOG_ERR 0x0016

/* Versioning: 0xMMmm, with 'M' for major and 'm' for minor */
#define TUNTAP_VERSION_MAJOR 0
#define TUNTAP_VERSION_MINOR 3
#define TUNTAP_VERSION ((TUNTAP_VERSION_MAJOR << 8) | TUNTAP_VERSION_MINOR)

#define TUNTAP_GET_FD(x) (x)->tun_fd

/* Handle Windows symbols export */
#if defined Windows
#if defined(tuntap_EXPORTS) && defined(_USRDLL) /* CMake generated goo */
#define TUNTAP_EXPORT __declspec(dllexport)
#elif defined(tuntap_EXPORTS)
#define TUNTAP_EXPORT __declspec(dllimport)
#else
#define TUNTAP_EXPORT extern
#endif
#else /* Unix */
#define TUNTAP_EXPORT extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  struct device
  {
    /** set me on ios and android to block on a promise for the fd */
    int (*obtain_fd)(struct device*);
    /** user data */
    void* user;
    t_tun tun_fd;
    int ctrl_sock;
    int flags; /* ifr.ifr_flags on Unix */
    char if_name[IF_NAMESIZE];
#if defined(Windows)
    int idx;        /* needed to set ipv6 address */
    DWORD bindaddr; /* set DNS client address */
#endif
#if defined(FreeBSD)
    int mode;
#endif
#if defined(__sun)
    int ip_fd;
    int reserved;
    char internal_name[IF_NAMESIZE];
#endif
  };

  /* User definable log callback */
  typedef void (*t_tuntap_log)(int, int, const char*, const char*);
  TUNTAP_EXPORT t_tuntap_log __tuntap_log;

#ifndef LOG_TAG
#define LOG_TAG "tuntap"
#endif

#define tuntap_log(lvl, msg) __tuntap_log(lvl, __LINE__, LOG_TAG, msg)

  /* Portable "public" functions */
  TUNTAP_EXPORT struct device*
  tuntap_init(void);
  TUNTAP_EXPORT int
  tuntap_version(void);
  TUNTAP_EXPORT void
  tuntap_destroy(struct device*);
  TUNTAP_EXPORT void
  tuntap_release(struct device*);
  TUNTAP_EXPORT int
  tuntap_start(struct device*, int, int);
  TUNTAP_EXPORT char*
  tuntap_get_ifname(struct device*);
  TUNTAP_EXPORT int
  tuntap_set_ifname(struct device*, const char*);

  TUNTAP_EXPORT int
  tuntap_set_descr(struct device*, const char*);
  TUNTAP_EXPORT int
  tuntap_up(struct device*);
  TUNTAP_EXPORT int
  tuntap_down(struct device*);
  TUNTAP_EXPORT int
  tuntap_get_mtu(struct device*);
  TUNTAP_EXPORT int
  tuntap_set_mtu(struct device*, int);

  /** set ip address and netmask

   */
  TUNTAP_EXPORT int
  tuntap_set_ip(struct device*, const char* srcaddr, const char* dstaddr, int netmask);
  // TUNTAP_EXPORT int		 tuntap_set_ip_old(struct device *, const char
  // *, int);
  /*TUNTAP_EXPORT int		 tuntap_set_ip_old(struct device *, const char
   * *, int);*/
  TUNTAP_EXPORT int
  tuntap_read(struct device*, void*, size_t);
  TUNTAP_EXPORT int
  tuntap_write(struct device*, void*, size_t);
  TUNTAP_EXPORT int
  tuntap_get_readable(struct device*);
  TUNTAP_EXPORT int
  tuntap_set_nonblocking(struct device* dev, int);
  TUNTAP_EXPORT int
  tuntap_set_debug(struct device* dev, int);

  /* Logging functions */
  TUNTAP_EXPORT void
  tuntap_log_set_cb(t_tuntap_log cb);
  void
  tuntap_log_default(int, int, const char*, const char*);
  void
  tuntap_log_hexdump(void*, size_t);
  void
  tuntap_log_chksum(void*, int);

  /* OS specific functions */
  int
  tuntap_sys_start(struct device*, int, int);
  void
  tuntap_sys_destroy(struct device*);
  int
  tuntap_sys_set_ipv4(struct device*, t_tun_in_addr*, uint32_t);

#if defined(Windows)
  int
  tuntap_sys_set_dns(struct device* dev, t_tun_in_addr* s, uint32_t mask);
#endif

#if defined(FreeBSD)
  int
  tuntap_sys_set_ipv4_tap(struct device*, t_tun_in_addr*, uint32_t);
  int
  tuntap_sys_set_ipv4_tun(
      struct device* dev, t_tun_in_addr* s4, t_tun_in_addr* s4dest, uint32_t bits, int netmask);
#endif

  int
  tuntap_sys_set_ipv6(struct device*, t_tun_in6_addr*, uint32_t);
  int
  tuntap_sys_set_ifname(struct device*, const char*, size_t);
  int
  tuntap_sys_set_descr(struct device*, const char*, size_t);

#ifdef __cplusplus
}
#endif

#endif

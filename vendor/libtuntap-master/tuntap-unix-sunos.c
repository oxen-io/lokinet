/*
 * Copyright (c) 2012 Tristan Le Guern <tleguern@bouledef.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * solaris port of libtuntap (c)2019 rick v
 * all rights reserved, see LICENSE in top-level folder (../../LICENSE)
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tuntap.h"

/* TODO(despair): port all this shit */

static int
tuntap_sys_create_dev(struct device *dev, int tun)
{
  return -1;
}

int
tuntap_sys_start(struct device *dev, int mode, int tun)
{
  return -1;
}

void
tuntap_sys_destroy(struct device *dev)
{
  return /*-1*/;
}

int
tuntap_sys_set_hwaddr(struct device *dev, struct ether_addr *eth_addr)
{
  return -1;
}

int
tuntap_sys_set_ipv4(struct device *dev, t_tun_in_addr *s4, uint32_t imask)
{
  return -1;
}

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s6, uint32_t imask)
{
  return -1;
}

int
tuntap_sys_set_ifname(struct device *dev, const char *ifname, size_t len) 
{
  return -1;
}

int
tuntap_sys_set_descr(struct device *dev, const char *descr, size_t len)
{
  (void)dev;
  (void)descr;
  (void)len;
  return -1;
}

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
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>

#include <string.h>

#include "tuntap.h"

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s6, uint32_t bits) {
	(void)dev;
	(void)s6;
	(void)bits;
	tuntap_log(TUNTAP_LOG_NOTICE, "IPv6 is not implemented on your system");
	return -1;
}

#ifndef SIOCSIFNAME
int
tuntap_sys_set_ifname(struct device *dev, const char *ifname, size_t len)
{
  (void)dev;
  (void)ifname;
  (void)len;
  tuntap_log(TUNTAP_LOG_NOTICE,
             "Your system does not support tuntap_set_ifname()");
  /* just leave it as tunX, there doesn't seem to be any
   * practical manner of setting this param in NetBSD and its forks :-( 
   */
  return 0;
}

#endif
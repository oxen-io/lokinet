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
#include <sys/sockio.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stropts.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <alloca.h>

#include "tuntap.h"

/* TODO(despair): port all this shit */

static int
tuntap_sys_create_dev(struct device *dev, int tun)
{
	int fd, strm_fd, ip_muxid, ppa = -1;
	struct lifreq lifr;
	struct ifreq ifr;
	const char *ptr = NULL;
	struct strioctl strioc_ppa;

	/* improved generic TUN/TAP driver from
	 * http://www.whiteboard.ne.jp/~admin2/tuntap/
	 * has IPv6 support. Most open-source variants of
	 * Solaris already have this driver in their package
	 * repos, Oracle Solaris users need to compile/load
	 * manually.
	 */

	explicit_bzero(&lifr, sizeof lifr);

	if ((dev->ip_fd = open("/dev/udp", O_RDWR, 0)) < 0)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't open /dev/udp");
		return -1;
	}

	if ((fd = open("/dev/tun", O_RDWR, 0)) < 0)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't open /dev/tun");
		return -1;
	}

	/* get unit number */
	if (*dev->if_name)
	{
		ptr = dev->if_name;
		while (*ptr && !isdigit((int) *ptr))
		{
			ptr++;
		}
		ppa = atoi(ptr);
	}

	/* Assign a new PPA and get its unit number. */
	strioc_ppa.ic_cmd = TUNNEWPPA;
	strioc_ppa.ic_timout = 0;
	strioc_ppa.ic_len = sizeof(ppa);
	strioc_ppa.ic_dp = (char *)&ppa;

	if (*ptr == '\0')           /* no number given, try dynamic */
	{
		bool found_one = false;
		while (!found_one && ppa < 64)
		{
			int new_ppa = ioctl(fd, I_STR, &strioc_ppa);
			if (new_ppa >= 0)
			{
				char* msg = alloca(512);
				sprintf(msg, "got dynamic interface tun%i", new_ppa);
				tuntap_log( TUNTAP_LOG_INFO, msg );
				ppa = new_ppa;
				found_one = true;
				break;
			}
			if (errno != EEXIST)
			{
				tuntap_log(TUNTAP_LOG_ERR, "unexpected error trying to find free tun interface");
				return -1;
			}
			ppa++;
		}
		if (!found_one)
		{
			tuntap_log(TUNTAP_LOG_ERR, "could not find free tun interface, give up.");
			return -1;
		}
	}
	else                        /* try this particular one */
	{
		if ((ppa = ioctl(fd, I_STR, &strioc_ppa)) < 0)
		{
			char *msg = alloca(512);
			sprintf(msg, "Can't assign PPA for new interface (tun%i)", ppa);
			tuntap_log(TUNTAP_LOG_ERR, msg);
			return -1;
		}
	}

	// Open a new handle to link up the STREAMS
	if ((strm_fd = open("/dev/tun", O_RDWR, 0)) < 0)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't open /dev/tun (2)");
		return -1;
	}

	if (ioctl(strm_fd, I_PUSH, "ip") < 0)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't push IP module");
		return -1;
	}

	/* Assign ppa according to the unit number returned by tun device */
	if (ioctl(strm_fd, IF_UNITSEL, (char *) &ppa) < 0)
	{
		char *msg = alloca(512);
		sprintf(msg, "Can't set PPA %i", ppa);
		tuntap_log(TUNTAP_LOG_ERR, msg);
		return -1;
	}

	snprintf(dev->internal_name, IF_NAMESIZE, "%s%d", "tun", ppa);

	if ((ip_muxid = ioctl(dev->ip_fd, I_PLINK, strm_fd)) < 0)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't link tun device to IP");
		return -1;
	}

	explicit_bzero(&lifr, sizeof lifr);
	explicit_bzero(&ifr, sizeof ifr);
	memcpy(lifr.lifr_name, dev->internal_name, sizeof(lifr.lifr_name));
	lifr.lifr_ip_muxid  = ip_muxid;

	if (ioctl(dev->ip_fd, SIOCSLIFMUXID, &lifr) < 0)
	{
		ioctl(dev->ip_fd, I_PUNLINK, ip_muxid);
		tuntap_log(TUNTAP_LOG_ERR, "Can't set multiplexor id");
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	fcntl(dev->ip_fd, F_SETFD, FD_CLOEXEC);
	char *msg = alloca(512);
	sprintf(msg, "TUN device %s opened as %s", dev->if_name, dev->internal_name);
	tuntap_log(TUNTAP_LOG_INFO, msg);

	(void)memcpy(ifr.ifr_name, dev->internal_name, sizeof dev->internal_name);

	/* Get the interface default values */
	if(ioctl(dev->ctrl_sock, SIOCGIFFLAGS, &ifr) == -1)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't get interface values");
		return -1;
	}
	/* Save flags for tuntap_{up, down} */
	dev->flags = ifr.ifr_flags;
	dev->reserved = strm_fd;
	return fd;
}

int
tuntap_sys_start(struct device *dev, int mode, int tun) 
{
	/* Forces automatic selection of device instance
	 * in tuntap_sys_create_dev().
	 * This also clears the specified interface name.
	 */
	if (tun == TUNTAP_ID_ANY)
		memset(&dev->if_name, '\0', sizeof dev->if_name);

	if (mode == TUNTAP_MODE_TUNNEL)
	{
		return tuntap_sys_create_dev(dev, tun);
	}
	else
		return -1;
	/* NOTREACHED */
}


void
tuntap_sys_destroy(struct device *dev)
{
	struct lifreq ifr;

	explicit_bzero(&ifr, sizeof ifr);
	strncpy(ifr.lifr_name, dev->internal_name, sizeof(ifr.lifr_name));

	if (ioctl(dev->ip_fd, SIOCGLIFFLAGS, &ifr) < 0)
	{
		tuntap_log(TUNTAP_LOG_WARN, "Can't get iface flags");
	}

	if (ioctl(dev->ip_fd, SIOCGLIFMUXID, &ifr) < 0)
	{
		tuntap_log(TUNTAP_LOG_WARN, "Can't get multiplexor id");
	}

	/* we don't support TAP, and i think jaff stripped out TAP code a while 
	 * back...
	 */

	if (ioctl(dev->ip_fd, I_PUNLINK, ifr.lifr_ip_muxid) < 0)
	{
		tuntap_log(TUNTAP_LOG_WARN, "Can't unlink interface(ip)");
	}

	close(dev->ip_fd);
	close(dev->reserved);
	dev->reserved = -1;
	dev->ip_fd = -1;
}


int
tuntap_sys_set_ipv4(struct device *dev, t_tun_in_addr *s4, uint32_t bits) 
{
	struct lifreq ifr;
	struct sockaddr_in mask;
	struct in_addr net;
	char *src, *dst, *netmask;

	(void)memset(&ifr, '\0', sizeof ifr);
	(void)memcpy(ifr.lifr_name, dev->internal_name, sizeof dev->internal_name);
	net.s_addr = htonl(ntohl(s4->s_addr) - 1); // this gets us x.x.x.0

	/* Set the IP address first */
	(void)memcpy(&(((struct sockaddr_in *)&ifr.lifr_addr)->sin_addr), s4,
		sizeof(struct in_addr));
	ifr.lifr_addr.ss_family = AF_INET;
	if(ioctl(dev->ctrl_sock, SIOCSLIFADDR, &ifr) == -1)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't set IP address");
		return -1;
	}

	/* Reinit the struct ifr */
	(void)memset(&ifr.lifr_addr, '\0', sizeof ifr.lifr_addr);

	/* Set the tunnel endpoint */
	(void)memcpy(&(((struct sockaddr_in *)&ifr.lifr_dstaddr)->sin_addr), s4,
		sizeof(struct in_addr));
	ifr.lifr_addr.ss_family = AF_INET;
	if(ioctl(dev->ctrl_sock, SIOCSLIFDSTADDR, &ifr) == -1)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't set IP address");
		return -1;
	}

	/* Reinit the struct ifr */
	(void)memset(&ifr.lifr_addr, '\0', sizeof ifr.lifr_addr);
	(void)memset(&ifr.lifr_addr, '\0', sizeof ifr.lifr_dstaddr);

	/* Then set the netmask */
	(void)memset(&mask, '\0', sizeof mask);
	mask.sin_family      = AF_INET;
	mask.sin_addr.s_addr = bits;
	(void)memcpy(&ifr.lifr_addr, &mask, sizeof mask);
	if(ioctl(dev->ctrl_sock, SIOCSLIFNETMASK, &ifr) == -1)
	{
		tuntap_log(TUNTAP_LOG_ERR, "Can't set netmask");
		return -1;
	}

	// Now set the route, yup even ovpn does this :-/
	char* cmd = alloca(512);
	src = alloca(16);
	dst = alloca(16);
	netmask = alloca(16);
	strlcpy(src, inet_ntoa(net), 16);
	strlcpy(dst, inet_ntoa(*s4), 16);
	strlcpy(netmask, inet_ntoa(mask.sin_addr), 16);
	sprintf(cmd, "route add %s -netmask %s %s 0", src, netmask, dst);
	return system(cmd);
}

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s6, uint32_t imask)
{
	(void)dev;
	(void)s6;
	(void)imask;
	tuntap_log(TUNTAP_LOG_NOTICE, "IPv6 is configured manually, this is currently unsupported");
	return -1;
}

int
tuntap_sys_set_ifname(struct device *dev, const char *ifname, size_t len)
{
	/* Not quite sure if solaris SIOCSLIFNAME work the same way as on Linux,
	 * given the correct parameters.
	 */
	(void)dev;
	(void)ifname;
	(void)len;
	tuntap_log(TUNTAP_LOG_NOTICE,
		"Your system does not support tuntap_set_ifname()");
	return -1;
}

int
tuntap_sys_set_descr(struct device *dev, const char *descr, size_t len)
{
	(void)dev;
	(void)descr;
	(void)len;
	tuntap_log(TUNTAP_LOG_NOTICE,
		"Your system does not support tuntap_set_descr()");
	return -1;
}

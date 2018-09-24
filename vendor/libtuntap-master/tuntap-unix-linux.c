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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <net/if_arp.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tuntap.h"

int
tuntap_sys_start(struct device *dev, int mode, int tun)
{
  int fd;
  int persist;
  char *ifname;
  struct ifreq ifr;

  /* Get the persistence bit */
  if(mode & TUNTAP_MODE_PERSIST)
  {
    mode &= ~TUNTAP_MODE_PERSIST;
    persist = 1;
  }
  else
  {
    persist = 0;
  }

  /* Set the mode: tun or tap */
  (void)memset(&ifr, '\0', sizeof ifr);
  if(mode == TUNTAP_MODE_ETHERNET)
  {
    ifr.ifr_flags = IFF_TAP;
    ifname        = "tap%i";
  }
  else if(mode == TUNTAP_MODE_TUNNEL)
  {
    ifr.ifr_flags = IFF_TUN;
    if(dev->if_name[0])
      strncpy(ifr.ifr_name, dev->if_name, sizeof(ifr.ifr_name));
    else
      ifname = "tun%i";
  }
  else
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'mode'");
    return -1;
  }
  ifr.ifr_flags |= IFF_NO_PI;

  if(tun < 0)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'tun'");
    return -1;
  }

  /* Open the clonable interface */
  fd = -1;
  if((fd = open("/dev/net/tun", O_RDWR)) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't open /dev/net/tun");
    return -1;
  }

  /* Set the interface name, if any */

  if(fd > TUNTAP_ID_MAX)
  {
    return -1;
  }
  if(ifr.ifr_name[0] == 0 && tun)
    (void)snprintf(ifr.ifr_name, sizeof ifr.ifr_name, ifname, tun);
  /* Save interface name *after* SIOCGIFFLAGS */

  /* Configure the interface */
  if(ioctl(fd, TUNSETIFF, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set interface name");
    return -1;
  }

  /* Set it persistent if needed */
  if(persist == 1)
  {
    if(ioctl(fd, TUNSETPERSIST, 1) == -1)
    {
      tuntap_log(TUNTAP_LOG_ERR, "Can't set persistent");
      return -1;
    }
  }

  /* Get the interface default values */
  if(ioctl(dev->ctrl_sock, SIOCGIFFLAGS, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't get interface values");
    return -1;
  }

  /* Save flags for tuntap_{up, down} */
  dev->flags = ifr.ifr_flags;

  /* Save interface name */
  (void)memcpy(dev->if_name, ifr.ifr_name, sizeof ifr.ifr_name);

  /* Save pre-existing MAC address */
  if(mode == TUNTAP_MODE_ETHERNET)
  {
    struct ifreq ifr_hw;

    (void)memcpy(ifr_hw.ifr_name, dev->if_name, sizeof(dev->if_name));
    if(ioctl(fd, SIOCGIFHWADDR, &ifr_hw) == -1)
    {
      tuntap_log(TUNTAP_LOG_WARN, "Can't get link-layer address");
      return fd;
    }
    (void)memcpy(dev->hwaddr, ifr_hw.ifr_hwaddr.sa_data, ETH_ALEN);
  }
  return fd;
}

void
tuntap_sys_destroy(struct device *dev)
{
  if(ioctl(dev->tun_fd, TUNSETPERSIST, 0) == -1)
  {
    tuntap_log(TUNTAP_LOG_WARN, "Can't destroy the interface");
  }
}

int
tuntap_sys_set_hwaddr(struct device *dev, struct ether_addr *eth_addr)
{
  struct ifreq ifr;

  (void)memset(&ifr, '\0', sizeof ifr);
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);

  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  (void)memcpy(ifr.ifr_hwaddr.sa_data, eth_addr->ether_addr_octet, 6);

  /* Linux has a special flag for setting the MAC address */
  if(ioctl(dev->ctrl_sock, SIOCSIFHWADDR, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set link-layer address");
    return -1;
  }
  return 0;
}

int
tuntap_sys_set_ipv4(struct device *dev, t_tun_in_addr *s4, uint32_t bits)
{
  struct ifreq ifr;
  struct sockaddr_in mask;

  (void)memset(&ifr, '\0', sizeof ifr);
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);

  /* Set the IP address first */
  (void)memcpy(&(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), s4,
               sizeof(struct in_addr));
  ifr.ifr_addr.sa_family = AF_INET;
  if(ioctl(dev->ctrl_sock, SIOCSIFADDR, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set IP address");
    return -1;
  }

  /* Reinit the struct ifr */
  (void)memset(&ifr.ifr_addr, '\0', sizeof ifr.ifr_addr);

  /* Then set the netmask */
  (void)memset(&mask, '\0', sizeof mask);
  mask.sin_family      = AF_INET;
  mask.sin_addr.s_addr = bits;
  (void)memcpy(&ifr.ifr_netmask, &mask, sizeof ifr.ifr_netmask);
  if(ioctl(dev->ctrl_sock, SIOCSIFNETMASK, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set netmask");
    return -1;
  }

  return 0;
}

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s6, uint32_t bits)
{
  (void)dev;
  (void)s6;
  (void)bits;
  tuntap_log(TUNTAP_LOG_NOTICE, "IPv6 is not implemented on your system");
  return -1;
}

int
tuntap_sys_set_ifname(struct device *dev, const char *ifname, size_t len)
{
  struct ifreq ifr;

  (void)strncpy(ifr.ifr_name, dev->if_name, IF_NAMESIZE);
  (void)strncpy(ifr.ifr_newname, ifname, len);

  if(ioctl(dev->ctrl_sock, SIOCSIFNAME, &ifr) == -1)
  {
    perror(NULL);
    tuntap_log(TUNTAP_LOG_ERR, "Can't set interface name");
    return -1;
  }
  return 0;
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

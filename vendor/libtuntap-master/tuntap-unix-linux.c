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
  char *ifname = NULL;
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
  fd = -1;
  if(dev->obtain_fd)
  {
    // for android
    fd = dev->obtain_fd(dev);
    if(fd == -1)
    {
      tuntap_log(TUNTAP_LOG_ERR, "failed to get network interface");
      return -1;
    }
    return fd;
  }
  else
  {
    /* Open the clonable interface */
    if((fd = open("/dev/net/tun", O_RDWR)) == -1)
    {
      tuntap_log(TUNTAP_LOG_ERR, "Can't open /dev/net/tun");
      return -1;
    }
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
#if defined(ANDROID)

#else
struct in6_ifreq {
  struct in6_addr ifr6_addr;
  __u32 ifr6_prefixlen;
  unsigned int ifr6_ifindex;
};
#endif

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s6, uint32_t bits)
{
  struct ifreq ifr;
  struct sockaddr_in6 sai;
  int sockfd;
  struct in6_ifreq ifr6;

  sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IP);
  if (sockfd == -1) {
    return -1;
  }

  /* get interface name */
  strncpy(ifr.ifr_name, dev->if_name, IFNAMSIZ);

  memset(&sai, 0, sizeof(struct sockaddr));
  sai.sin6_family = AF_INET6;
  sai.sin6_port = 0;

  memcpy((char *) &ifr6.ifr6_addr, (char *) s6,
sizeof(struct in6_addr));

  if (ioctl(sockfd, SIOGIFINDEX, &ifr) < 0) {
    perror("SIOGIFINDEX");
    close(sockfd);
    return -1;
  }
  ifr6.ifr6_ifindex = ifr.ifr_ifindex;
  ifr6.ifr6_prefixlen = bits;
  if (ioctl(sockfd, SIOCSIFADDR, &ifr6) < 0) {
    perror("SIOCSIFADDR");
    close(sockfd);
    return -1;
  }
  close(sockfd);
  return 0;
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

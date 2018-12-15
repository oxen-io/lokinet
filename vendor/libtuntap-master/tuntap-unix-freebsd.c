/*
 * Copyright (c) 2012 Tristan Le Guern <tleguern@bouledef.eu>
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
#include <sys/ioctl.h>
#include <sys/param.h> /* For MAXPATHLEN */
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/if.h>
#if defined FreeBSD
#include <net/if_tun.h>
#elif defined DragonFly
#include <net/tun/if_tun.h>
#endif
#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
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
  char name[MAXPATHLEN];
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
  if(mode == TUNTAP_MODE_ETHERNET)
  {
    ifname = "tap";
  }
  else if(mode == TUNTAP_MODE_TUNNEL)
  {
    ifname = "tun";
  }
  else
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'mode'");
    return -1;
  }

  dev->mode = mode;

  /* Try to use the given driver or loop throught the avaible ones */
  fd = -1;
  if(tun < TUNTAP_ID_MAX)
  {
    (void)snprintf(name, sizeof(name), "/dev/%s%i", ifname, tun);
    fd = open(name, O_RDWR);
  }
  else if(tun == TUNTAP_ID_ANY)
  {
    for(tun = 0; tun < TUNTAP_ID_MAX; ++tun)
    {
      (void)memset(name, 0, sizeof(name));
      (void)snprintf(name, sizeof(name), "/dev/%s%i", ifname, tun);
      if((fd = open(name, O_RDWR)) > 0)
        break;
    }
  }
  else
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'tun'");
    return -1;
  }
  switch(fd)
  {
    case -1:
      tuntap_log(TUNTAP_LOG_ERR, "Permission denied");
      return -1;
    case 256:
      tuntap_log(TUNTAP_LOG_ERR, "Can't find a tun entry");
      return -1;
    default:
      /* NOTREACHED */
      break;
  }
  char newifname[IFNAMSIZ] = {0};
  (void)strlcpy(newifname, dev->if_name, sizeof(newifname));

  /* Set the interface name */
  (void)memset(&ifr, 0, sizeof(ifr));
  (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s%i", ifname, tun);
  /* And save it */
  (void)strlcpy(dev->if_name, ifr.ifr_name, sizeof(dev->if_name));

  /* Get the interface default values */
  if(ioctl(dev->ctrl_sock, SIOCGIFFLAGS, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't get interface values");
    return -1;
  }

  int set = 1;
  if(ioctl(dev->tun_fd, TUNSIFHEAD, &set) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "ioctl for TUNSIFHEAD failed");
    return -1;
  }

  /* Save flags for tuntap_{up, down} */
  dev->flags = ifr.ifr_flags;

  return fd;
}

void
tuntap_sys_destroy(struct device *dev)
{
  char cmdbuf[128] = {0};
  snprintf(cmdbuf, sizeof(cmdbuf), "ifconfig %s destroy", dev->if_name);
  tuntap_log(TUNTAP_LOG_INFO, cmdbuf);
  system(cmdbuf);
  return;
}

int
tuntap_sys_set_ipv4_tap(struct device *dev, t_tun_in_addr *s4, uint32_t bits)
{
  struct ifaliasreq ifrq;
  struct sockaddr_in mask;
  struct sockaddr_in addr;
  struct ifreq ifr;

  (void)memset(&ifrq, 0, sizeof(ifrq));
  (void)strlcpy(ifrq.ifra_name, dev->if_name, sizeof(ifr.ifr_name));

  /* Delete previously assigned address */
  (void)ioctl(dev->ctrl_sock, SIOCDIFADDR, &ifrq);

  /* Set the address */
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = s4->s_addr;
  addr.sin_len         = sizeof(addr);
  (void)memcpy(&ifrq.ifra_addr, &addr, sizeof(addr));

  /* Then set the netmask */
  (void)memset(&mask, 0, sizeof(mask));
  mask.sin_family      = AF_INET;
  mask.sin_addr.s_addr = bits;
  mask.sin_len         = sizeof(mask);
  (void)memcpy(&ifrq.ifra_addr, &mask, sizeof(ifrq.ifra_mask));

  if(ioctl(dev->ctrl_sock, SIOCAIFADDR, &ifrq) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set IP address/netmask");
    return -1;
  }
  return 0;
}

static int
tuntap_sys_add_route(struct device *dev, t_tun_in_addr *s4, uint32_t bits,
                     int netmask)
{
  struct sockaddr_in mask;
  mask.sin_family      = AF_INET;
  mask.sin_addr.s_addr = bits;
  mask.sin_len         = sizeof(struct sockaddr_in);
  char addrbuf[32]     = {0};
  char bcaddrbuf[32]   = {0};
  char buf[1028]       = {0};

  inet_ntop(AF_INET, s4, addrbuf, sizeof(struct sockaddr_in));

  const char *addr        = addrbuf;
  const char *netmask_str = inet_ntoa(mask.sin_addr);
  struct in_addr bca;
  bca.s_addr = s4->s_addr | ~mask.sin_addr.s_addr;
  inet_ntop(AF_INET, &bca, bcaddrbuf, sizeof(struct sockaddr_in));
  const char *bcaddr = bcaddrbuf;
  /** because fuck this other stuff */
  snprintf(buf, sizeof(buf), "ifconfig %s %s %s mtu 1380 netmask %s up",
           dev->if_name, addr, bcaddr, netmask_str);
  tuntap_log(TUNTAP_LOG_INFO, buf);
  system(buf);
  snprintf(buf, sizeof(buf), "route add %s/%d -interface %s", addr, netmask,
           dev->if_name);
  tuntap_log(TUNTAP_LOG_INFO, buf);
  system(buf);
  return 0;
}

int
tuntap_sys_set_ipv4_tun(struct device *dev, t_tun_in_addr *s4,
                        t_tun_in_addr *s4dest, uint32_t bits, int netmask)
{
  struct ifaliasreq ifrq;
  struct sockaddr_in mask;
  struct sockaddr_in saddr;
  struct sockaddr_in daddr;

  (void)memset(&ifrq, 0, sizeof(ifrq));
  (void)memcpy(ifrq.ifra_name, dev->if_name, sizeof(ifrq.ifra_name));

  /* Delete previously assigned address */
  (void)ioctl(dev->ctrl_sock, SIOCDIFADDR, &ifrq);

  /* Set the address */
  (void)memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family      = AF_INET;
  saddr.sin_addr.s_addr = s4->s_addr;
  saddr.sin_len         = sizeof(saddr);
  (void)memcpy(&ifrq.ifra_addr, &saddr, sizeof(saddr));

  (void)memset(&daddr, 0, sizeof(daddr));
  daddr.sin_family      = AF_INET;
  daddr.sin_addr.s_addr = s4dest->s_addr;
  daddr.sin_len         = sizeof(daddr);
  (void)memcpy(&ifrq.ifra_broadaddr, &daddr, sizeof(daddr));

  /* Then set the netmask */
  (void)memset(&mask, 0, sizeof(mask));
  mask.sin_family      = AF_INET;
  mask.sin_addr.s_addr = bits;
  mask.sin_len         = sizeof(mask);
  (void)memcpy(&ifrq.ifra_addr, &mask, sizeof(ifrq.ifra_mask));

  if(ioctl(dev->ctrl_sock, SIOCAIFADDR, &ifrq) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set IP address");
    return -1;
  }
  return tuntap_sys_add_route(dev, s4, bits, netmask);
}

int
tuntap_sys_set_descr(struct device *dev, const char *descr, size_t len)
{
#if defined FreeBSD
  struct ifreq ifr;
  struct ifreq_buffer ifrbuf;

  (void)memset(&ifr, 0, sizeof(ifr));
  (void)strlcpy(ifr.ifr_name, dev->if_name, sizeof(ifr.ifr_name));

  ifrbuf.buffer  = (void *)descr;
  ifrbuf.length  = len;
  ifr.ifr_buffer = ifrbuf;

  if(ioctl(dev->ctrl_sock, SIOCSIFDESCR, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set the interface description");
    return -1;
  }
  return 0;
#elif defined DragonFly
  tuntap_log(TUNTAP_LOG_NOTICE,
             "Your system does not support tuntap_set_descr()");
  return -1;
#endif
}

int
tuntap_sys_set_ifname(struct device *dev, const char *ifname, size_t len)
{
  struct ifreq ifr;
  char *newname;
  //(void)strncpy(ifr.ifr_name, dev->if_name, IF_NAMESIZE);
  strlcpy(ifr.ifr_name, dev->if_name, IF_NAMESIZE);

  newname = strdup(ifname);
  if(newname == NULL)
  {
    tuntap_log(TUNTAP_LOG_ERR, "no memory to set ifname");
    return -1;
  }
  ifr.ifr_data = newname;
  if(ioctl(dev->ctrl_sock, SIOCSIFNAME, &ifr) == -1)
  {
    perror(NULL);
    free(newname);
    tuntap_log(TUNTAP_LOG_ERR, "Can't set interface name");
    return -1;
  }
  (void)strlcpy(dev->if_name, ifname, len);
  free(newname);
  return 0;
}

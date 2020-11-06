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

#include <tuntap.h>

#ifdef __sun
#define BSD_COMP
#define TUNSDEBUG _IOW('t', 90, int)
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#if defined(Linux)
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#else

#include <net/if.h>
#if defined(DragonFly)
#include <net/tun/if_tun.h>
#elif defined(ANDROID)
#include <linux/if_tun.h>
#elif defined(Darwin)
#include <sys/uio.h>
#include <unistd.h>
#else
#include <net/if_tun.h>
#endif
#include <netinet/if_ether.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


int
tuntap_start(struct device *dev, int mode, int tun)
{
  int sock;
  int fd;

  fd = sock = -1;

  /* Don't re-initialise a previously started device */
  if(dev->tun_fd != -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Device is already started");
    return -1;
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock == -1)
  {
    goto clean;
  }
  dev->ctrl_sock = sock;

  if(mode & TUNTAP_MODE_PERSIST && tun == TUNTAP_ID_ANY)
  {
    goto clean; /* XXX: Explain why */
  }

  fd = tuntap_sys_start(dev, mode, tun);
  if(fd == -1)
  {
    goto clean;
  }

  dev->tun_fd = fd;
  return 0;

clean:
  if(fd != -1)
  {
    (void)close(fd);
  }
  if(sock != -1)
  {
    (void)close(sock);
  }
  return -1;
}

void
tuntap_release(struct device *dev)
{
  (void)close(dev->tun_fd);
  (void)close(dev->ctrl_sock);
}

int
tuntap_set_descr(struct device *dev, const char *descr)
{
  size_t len;

  if(descr == NULL)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'descr'");
    return -1;
  }

  len = strlen(descr);
  if(len > IF_DESCRSIZE)
  {
    /* The value will be troncated */
    tuntap_log(TUNTAP_LOG_WARN, "Parameter 'descr' is too long");
  }

  if(tuntap_sys_set_descr(dev, descr, len) == -1)
  {
    return -1;
  }
  return 0;
}

int
tuntap_set_ifname(struct device *dev, const char *ifname)
{
  size_t len;

  if(ifname == NULL)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'ifname'");
    return -1;
  }

  len = strlen(ifname);
  if(len >= IF_NAMESIZE)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Parameter 'ifname' is too long");
    return -1;
  }

  if(tuntap_sys_set_ifname(dev, ifname, len) == -1)
  {
    return 0;
  }

  (void)memset(dev->if_name, 0, IF_NAMESIZE);
  (void)strncpy(dev->if_name, ifname, len + 1);
  return 0;
}

int
tuntap_up(struct device *dev)
{
  struct ifreq ifr;

  (void)memset(&ifr, '\0', sizeof ifr);
#ifndef __sun
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);
#else
  (void)memcpy(ifr.ifr_name, dev->internal_name, sizeof dev->internal_name);
#endif
  ifr.ifr_flags = (short int)dev->flags;
  ifr.ifr_flags |= IFF_UP;

  if(ioctl(dev->ctrl_sock, SIOCSIFFLAGS, &ifr) == -1)
  {
    return -1;
  }

  dev->flags = ifr.ifr_flags;
  return 0;
}

int
tuntap_down(struct device *dev)
{
  struct ifreq ifr;

  (void)memset(&ifr, '\0', sizeof ifr);
#ifndef __sun
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);
#else
  (void)memcpy(ifr.ifr_name, dev->internal_name, sizeof dev->internal_name);
#endif
  ifr.ifr_flags = (short)dev->flags;
  ifr.ifr_flags &= ~IFF_UP;

  if(ioctl(dev->ctrl_sock, SIOCSIFFLAGS, &ifr) == -1)
  {
    return -1;
  }

  dev->flags = ifr.ifr_flags;
  return 0;
}

int
tuntap_get_mtu(struct device *dev)
{
  struct ifreq ifr;

  /* Only accept started device */
  if(dev->tun_fd == -1)
  {
    tuntap_log(TUNTAP_LOG_NOTICE, "Device is not started");
    return 0;
  }

  (void)memset(&ifr, '\0', sizeof ifr);
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);

  if(ioctl(dev->ctrl_sock, SIOCGIFMTU, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't get MTU");
    return -1;
  }
  return ifr.ifr_mtu;
}

int
tuntap_set_mtu(struct device *dev, int mtu)
{
  struct ifreq ifr;

  /* Only accept started device */
  if(dev->tun_fd == -1)
  {
    tuntap_log(TUNTAP_LOG_NOTICE, "Device is not started");
    return 0;
  }

  (void)memset(&ifr, '\0', sizeof ifr);
  (void)memcpy(ifr.ifr_name, dev->if_name, sizeof dev->if_name);
  ifr.ifr_mtu = mtu;

  if(ioctl(dev->ctrl_sock, SIOCSIFMTU, &ifr) == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "Can't set MTU");
    return -1;
  }
  return 0;
}

int
tuntap_read(struct device *dev, void *buf, size_t size)
{
  int n;

  /* Only accept started device */
  if(dev->tun_fd == -1)
  {
    tuntap_log(TUNTAP_LOG_NOTICE, "Device is not started");
    return 0;
  }
#ifdef Darwin
  unsigned int pktinfo       = 0;
  const struct iovec vecs[2] = {
      {.iov_base = &pktinfo, .iov_len = sizeof(unsigned int)},
      {.iov_base = buf, .iov_len = size}};
  n = readv(dev->tun_fd, vecs, 2);
  if(n >= (int)(sizeof(unsigned int)))
    n -= sizeof(unsigned int);
#else
  n = read(dev->tun_fd, buf, size);
#endif
  if(n == -1)
  {
    tuntap_log(TUNTAP_LOG_WARN, "Can't to read from device");
    return -1;
  }
  return n;
}

int
tuntap_write(struct device *dev, void *buf, size_t size)
{
  int n;

  /* Only accept started device */
  if(dev->tun_fd == -1)
  {
    tuntap_log(TUNTAP_LOG_NOTICE, "Device is not started");
    return 0;
  }
#if defined(Darwin)
  /** darwin has packet info so let's use writev */
  static unsigned int af4 = htonl(AF_INET);
  static unsigned int af6 = htonl(AF_INET6);

  const struct iovec vecs[2] = {
      {.iov_base = (((unsigned char *)buf)[0] & 0x60) == 0x60 ? &af6 : &af4,
       .iov_len  = sizeof(unsigned int)},
      {.iov_base = buf, .iov_len = size}};

  n = writev(dev->tun_fd, vecs, 2);
  if(n >= (int)sizeof(unsigned int))
    n -= sizeof(unsigned int);

#else
  n = write(dev->tun_fd, buf, size);
#endif
  if(n < 0)
  {
    tuntap_log(TUNTAP_LOG_WARN, "Can't write to device");
    return -1;
  }
  return n;
}

int
tuntap_get_readable(struct device *dev)
{
  int n;

  n = 0;
  if(ioctl(dev->tun_fd, FIONREAD, &n) == -1)
  {
    tuntap_log(TUNTAP_LOG_INFO,
               "Your system does not support"
               " FIONREAD, fallback to MTU");
    return tuntap_get_mtu(dev);
  }
  return n;
}

int
tuntap_set_nonblocking(struct device *dev, int set)
{
  if(ioctl(dev->tun_fd, FIONBIO, &set) == -1)
  {
    switch(set)
    {
      case 0:
        tuntap_log(TUNTAP_LOG_ERR, "Can't unset nonblocking");
        break;
      case 1:
        tuntap_log(TUNTAP_LOG_ERR, "Can't set nonblocking");
        break;
      default:
        tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'set'");
    }
    return -1;
  }
  return 0;
}

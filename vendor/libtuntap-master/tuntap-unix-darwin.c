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
#include <sys/param.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "tuntap.h"

#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/kern_event.h>

#define APPLE_UTUN "com.apple.net.utun_control"
#define UTUN_OPT_IFNAME 2

static int
fucky_tuntap_sys_start(struct device *dev, int mode, int tun)
{
  uint32_t namesz = IFNAMSIZ;
  char name[IFNAMSIZ + 1];
  int fd;
  char *ifname;

  fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  if(fd == -1)
    return fd;

  snprintf(name, sizeof(name), "utun%i", tun);

  struct ctl_info info;
  memset(&info, 0, sizeof(info));
  strncpy(info.ctl_name, APPLE_UTUN, strlen(APPLE_UTUN));

  if(ioctl(fd, CTLIOCGINFO, &info) < 0)
  {
    tuntap_log(TUNTAP_LOG_ERR, "call to ioctl() failed");
    tuntap_log(TUNTAP_LOG_ERR, strerror(errno));
    close(fd);
    return -1;
  }

  struct sockaddr_ctl addr;
  addr.sc_id = info.ctl_id;

  addr.sc_len     = sizeof(addr);
  addr.sc_family  = AF_SYSTEM;
  addr.ss_sysaddr = AF_SYS_CONTROL;
  addr.sc_unit    = tun + 1;

  if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    close(fd);
    return -1;
  }
  ifname = name;
  if(getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &namesz) < 0)
  {
    close(fd);
    return -1;
  }
  strncpy(dev->if_name, ifname, sizeof(dev->if_name));

  return fd;
}

int
tuntap_sys_start(struct device *dev, int mode, int tun)
{
  int fd = -1;
  while(tun < 128)
  {
    // yes linear complexity here
    // sue me but I blame apple
    fd = fucky_tuntap_sys_start(dev, mode, tun);
    if(fd != -1)
    {
      return fd;
    }
    ++tun;
  }
  return -1;
}

void
tuntap_sys_destroy(struct device *dev)
{
  (void)dev;
}

struct tuntap_rtmsg
{
  struct rt_msghdr hdr;
  struct sockaddr_in saddr;
  struct sockaddr_in mask;
  struct sockaddr_in daddr;
};

int
tuntap_sys_set_ipv4(struct device *dev, t_tun_in_addr *s4, uint32_t bits)
{
  struct sockaddr_in mask;
  mask.sin_family      = AF_INET;
  mask.sin_addr.s_addr = bits;
  mask.sin_len         = sizeof(struct sockaddr_in);
  char addrbuf[32];
  inet_ntop(AF_INET, s4, addrbuf, sizeof(struct sockaddr_in));
  char buf[1028];
  const char *addr    = addrbuf;
  const char *netmask = inet_ntoa(mask.sin_addr);
  /** because fuck this other stuff */
  snprintf(buf, sizeof(buf), "ifconfig %s %s %s mtu 1380 netmask %s up",
           dev->if_name, addr, addr, netmask);
  tuntap_log(TUNTAP_LOG_INFO, buf);
  system(buf);
  snprintf(buf, sizeof(buf),
           "route add -cloning -net %s -netmask %s -interface %s", addr,
           netmask, dev->if_name);
  tuntap_log(TUNTAP_LOG_INFO, buf);
  system(buf);
  /* Simpler than calling SIOCSIFADDR and/or SIOCSIFBRDADDR */
  /*
    if(ioctl(dev->ctrl_sock, SIOCSIFADDR, &ifa) == -1)
    {
      tuntap_log(TUNTAP_LOG_ERR, "Can't set IP");
      tuntap_log(TUNTAP_LOG_ERR, strerror(errno));
      return -1;
    }
  */

  /*

  int fd = socket(PF_ROUTE, SOCK_RAW, AF_INET);

  struct tuntap_rtmsg msg;
  memset(&msg, 0, sizeof(msg));
  msg.hdr.rtm_msglen = sizeof(msg) - sizeof(struct rt_msghdr);
  msg.hdr.rtm_version = RTM_VERSION;
  msg.hdr.rtm_type = RTM_ADD;
  msg.hdr.rtm_addrs = RTA_NETMASK | RTA_IFA | RTA_DST;
  msg.hdr.rtm_flags = RTF_UP | RTF_STATIC | RTF_IFSCOPE;
  msg.hdr.rtm_index = if_nametoindex(dev->if_name);
  msg.hdr.rtm_pid = getpid();

  msg.saddr.sin_addr.s_addr = s4->s_addr & bits;
  msg.saddr.sin_family = AF_INET;
  msg.saddr.sin_len = sizeof(struct sockaddr_in);

  msg.daddr.sin_addr.s_addr = s4->s_addr;
  msg.daddr.sin_family = AF_INET;
  msg.daddr.sin_len = sizeof(struct sockaddr_in);

  msg.mask.sin_addr.s_addr = bits;
  msg.mask.sin_family = AF_INET;
  msg.mask.sin_len = sizeof(struct sockaddr_in);

  int res = write(fd, &msg, sizeof(msg));
  if(res == -1)
  {
    tuntap_log(TUNTAP_LOG_ERR, "did not add route");
    tuntap_log(TUNTAP_LOG_ERR, strerror(errno));
  }
  close(fd);
  return res == -1 ? -1 : 0;
*/
  return 0;
}

int
tuntap_sys_set_descr(struct device *dev, const char *descr, size_t len)
{
  tuntap_log(TUNTAP_LOG_NOTICE,
             "Your system does not support tuntap_set_descr()");
  return -1;
}

#pragma once

#include <llarp/ev/vpn.hpp>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.hpp"
#include <linux/if.h>
#include <linux/if_tun.h>

namespace llarp::vpn
{
  struct in6_ifreq
  {
    in6_addr addr;
    uint32_t prefixlen;
    unsigned int ifindex;
  };

  class LinuxInterface : public NetworkInterface
  {
    const int m_fd;
    const InterfaceInfo m_Info;

   public:
    LinuxInterface(InterfaceInfo info)
        : NetworkInterface{}, m_fd{::open("/dev/net/tun", O_RDWR)}, m_Info{std::move(info)}

    {
      if (m_fd == -1)
        throw std::runtime_error("cannot open /dev/net/tun " + std::string{strerror(errno)});

      ifreq ifr{};
      in6_ifreq ifr6{};
      ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
      std::copy_n(
          m_Info.ifname.c_str(),
          std::min(m_Info.ifname.size(), sizeof(ifr.ifr_name)),
          ifr.ifr_name);
      if (::ioctl(m_fd, TUNSETIFF, &ifr) == -1)
        throw std::runtime_error("cannot set interface name: " + std::string{strerror(errno)});
      IOCTL control{AF_INET};

      control.ioctl(SIOCGIFFLAGS, &ifr);
      const int flags = ifr.ifr_flags;
      control.ioctl(SIOCGIFINDEX, &ifr);
      const int ifindex = ifr.ifr_ifindex;

      IOCTL control6{AF_INET6};
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          ifr.ifr_addr.sa_family = AF_INET;
          const nuint32_t addr = ToNet(net::TruncateV6(ifaddr.range.addr));
          ((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr = addr.n;
          control.ioctl(SIOCSIFADDR, &ifr);

          const nuint32_t mask = ToNet(net::TruncateV6(ifaddr.range.netmask_bits));
          ((sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr = mask.n;
          control.ioctl(SIOCSIFNETMASK, &ifr);
        }
        if (ifaddr.fam == AF_INET6)
        {
          ifr6.addr = net::HUIntToIn6(ifaddr.range.addr);
          ifr6.prefixlen = llarp::bits::count_bits(ifaddr.range.netmask_bits);
          ifr6.ifindex = ifindex;
          try
          {
            control6.ioctl(SIOCSIFADDR, &ifr6);
          }
          catch (permission_error& ex)
          {
            LogError("we are not allowed to use IPv6 on this system: ", ex.what());
          }
        }
      }
      ifr.ifr_flags = static_cast<short>(flags | IFF_UP | IFF_NO_PI);
      control.ioctl(SIOCSIFFLAGS, &ifr);
    }

    virtual ~LinuxInterface()
    {
      ::close(m_fd);
    }

    int
    PollFD() const override
    {
      return m_fd;
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt;
      const auto sz = read(m_fd, pkt.buf, sizeof(pkt.buf));
      if (sz >= 0)
        pkt.sz = std::min(sz, ssize_t{sizeof(pkt.buf)});
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
        pkt.sz = 0;
      else
        throw std::error_code{errno, std::system_category()};
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      const auto sz = write(m_fd, pkt.buf, pkt.sz);
      if (sz <= 0)
        return false;
      return sz == static_cast<ssize_t>(pkt.sz);
    }

    std::string
    IfName() const override
    {
      return m_Info.ifname;
    }
  };

  class LinuxPlatform : public Platform
  {
   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) override
    {
      return std::make_shared<LinuxInterface>(std::move(info));
    };
  };

}  // namespace llarp::vpn

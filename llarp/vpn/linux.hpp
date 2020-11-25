#pragma once

#include <ev/vpn.hpp>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vpn/common.hpp>
#include <linux/if_tun.h>

namespace llarp::vpn
{
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
      ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
      std::copy_n(
          m_Info.ifname.c_str(),
          std::min(m_Info.ifname.size(), sizeof(ifr.ifr_name)),
          ifr.ifr_name);
      if (::ioctl(m_fd, TUNSETIFF, &ifr) == -1)
        throw std::runtime_error("cannot set interface name: " + std::string{strerror(errno)});
      IOCTL control{};
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          ifr.ifr_addr.sa_family = AF_INET;
          const nuint32_t addr = xhtonl(net::TruncateV6(ifaddr.range.addr));
          ((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr = addr.n;
          control.ioctl(SIOCSIFADDR, &ifr);

          const nuint32_t mask = xhtonl(net::TruncateV6(ifaddr.range.netmask_bits));
          ((sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr = mask.n;
          control.ioctl(SIOCSIFNETMASK, &ifr);
        }
        // TODO: ipv6 somehow?
      }
      control.ioctl(SIOCGIFFLAGS, &ifr);
      ifr.ifr_flags |= IFF_UP | IFF_NO_PI;
      control.ioctl(SIOCSIFFLAGS, &ifr);
    }

    virtual ~LinuxInterface()
    {
      if (m_fd != -1)
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
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      const auto sz = write(m_fd, pkt.buf, pkt.sz);
      if (sz <= 0)
        return false;
      return sz == ssize_t{pkt.sz};
    }

    bool
    HasNextPacket() override
    {
      return false;
    }

    std::string
    Name() const override
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

#pragma once

#include <llarp/ev/vpn.hpp>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.hpp"
#include <net/if.h>
#include <linux/if_tun.h>

#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <llarp/net/net.hpp>
#include <llarp/util/str.hpp>
#include <exception>

#include <oxenc/endian.h>

#include <llarp/router/abstractrouter.hpp>
#include <llarp.hpp>

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
            IOCTL{AF_INET6}.ioctl(SIOCSIFADDR, &ifr6);
          }
          catch (std::exception& ex)
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

  class LinuxRouteManager : public IRouteManager
  {
    const int fd;

    enum class GatewayMode
    {
      eFirstHop,
      eLowerDefault,
      eUpperDefault
    };

    struct NLRequest
    {
      nlmsghdr n;
      rtmsg r;
      char buf[4096];

      void
      AddData(int type, const void* data, int alen)
      {
#define NLMSG_TAIL(nmsg) ((struct rtattr*)(((intptr_t)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

        int len = RTA_LENGTH(alen);
        rtattr* rta;
        if (NLMSG_ALIGN(n.nlmsg_len) + RTA_ALIGN(len) > sizeof(*this))
        {
          throw std::length_error{"nlrequest add data overflow"};
        }
        rta = NLMSG_TAIL(&n);
        rta->rta_type = type;
        rta->rta_len = len;
        if (alen)
        {
          memcpy(RTA_DATA(rta), data, alen);
        }
        n.nlmsg_len = NLMSG_ALIGN(n.nlmsg_len) + RTA_ALIGN(len);
#undef NLMSG_TAIL
      }
    };

    /* Helper structure for ip address data and attributes */
    struct _inet_addr
    {
      unsigned char family;
      unsigned char bitlen;
      unsigned char data[sizeof(struct in6_addr)];

      _inet_addr(huint32_t addr, size_t bits = 32)
      {
        family = AF_INET;
        bitlen = bits;
        oxenc::write_host_as_big(addr.h, data);
      }

      _inet_addr(huint128_t addr, size_t bits = 128)
      {
        family = AF_INET6;
        bitlen = bits;
        const nuint128_t net = ToNet(addr);
        std::memcpy(data, &net, 16);
      }
    };

    void
    Blackhole(int cmd, int flags, int af)
    {
      NLRequest nl_request{};
      /* Initialize request structure */
      nl_request.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
      nl_request.n.nlmsg_flags = NLM_F_REQUEST | flags;
      nl_request.n.nlmsg_type = cmd;
      nl_request.n.nlmsg_pid = getpid();
      nl_request.r.rtm_family = af;
      nl_request.r.rtm_table = RT_TABLE_LOCAL;
      nl_request.r.rtm_type = RTN_BLACKHOLE;
      nl_request.r.rtm_scope = RT_SCOPE_UNIVERSE;
      if (af == AF_INET)
      {
        uint32_t addr{};
        nl_request.AddData(RTA_DST, &addr, sizeof(addr));
      }
      else
      {
        uint128_t addr{};
        nl_request.AddData(RTA_DST, &addr, sizeof(addr));
      }
      send(fd, &nl_request, sizeof(nl_request), 0);
    }

    void
    Route(
        int cmd,
        int flags,
        const _inet_addr& dst,
        const _inet_addr& gw,
        GatewayMode mode,
        int if_idx)
    {
      NLRequest nl_request{};

      /* Initialize request structure */
      nl_request.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
      nl_request.n.nlmsg_flags = NLM_F_REQUEST | flags;
      nl_request.n.nlmsg_type = cmd;
      nl_request.n.nlmsg_pid = getpid();
      nl_request.r.rtm_family = dst.family;
      nl_request.r.rtm_table = RT_TABLE_MAIN;
      if (if_idx)
      {
        nl_request.r.rtm_scope = RT_SCOPE_LINK;
      }
      else
      {
        nl_request.r.rtm_scope = RT_SCOPE_NOWHERE;
      }
      /* Set additional flags if NOT deleting route */
      if (cmd != RTM_DELROUTE)
      {
        nl_request.r.rtm_protocol = RTPROT_BOOT;
        nl_request.r.rtm_type = RTN_UNICAST;
      }

      nl_request.r.rtm_family = dst.family;
      nl_request.r.rtm_dst_len = dst.bitlen;
      nl_request.r.rtm_scope = 0;

      /* Set gateway */
      if (gw.bitlen != 0 and dst.family == AF_INET)
      {
        nl_request.AddData(RTA_GATEWAY, &gw.data, gw.bitlen / 8);
      }
      nl_request.r.rtm_family = gw.family;
      if (mode == GatewayMode::eFirstHop)
      {
        nl_request.AddData(RTA_DST, &dst.data, dst.bitlen / 8);
        /* Set interface */
        nl_request.AddData(RTA_OIF, &if_idx, sizeof(int));
      }
      if (mode == GatewayMode::eUpperDefault)
      {
        if (dst.family == AF_INET)
        {
          nl_request.AddData(RTA_DST, &dst.data, sizeof(uint32_t));
        }
        else
        {
          nl_request.AddData(RTA_OIF, &if_idx, sizeof(int));
          nl_request.AddData(RTA_DST, &dst.data, sizeof(in6_addr));
        }
      }
      /* Send message to the netlink */
      send(fd, &nl_request, sizeof(nl_request), 0);
    }

    void
    DefaultRouteViaInterface(std::string ifname, int cmd, int flags)
    {
      int if_idx = if_nametoindex(ifname.c_str());
      const auto maybe = Net().GetInterfaceAddr(ifname);
      if (not maybe)
        throw std::runtime_error{"we dont have our own network interface?"};

      const _inet_addr gateway{maybe->asIPv4()};
      const _inet_addr lower{ipaddr_ipv4_bits(0, 0, 0, 0), 1};
      const _inet_addr upper{ipaddr_ipv4_bits(128, 0, 0, 0), 1};

      Route(cmd, flags, lower, gateway, GatewayMode::eLowerDefault, if_idx);
      Route(cmd, flags, upper, gateway, GatewayMode::eUpperDefault, if_idx);

      if (const auto maybe6 = Net().GetInterfaceIPv6Address(ifname))
      {
        const _inet_addr gateway6{*maybe6, 128};
        for (const std::string str : {"::", "4000::", "8000::", "c000::"})
        {
          huint128_t _hole{};
          _hole.FromString(str);
          const _inet_addr hole6{_hole, 2};
          Route(cmd, flags, hole6, gateway6, GatewayMode::eUpperDefault, if_idx);
        }
      }
    }

    void
    RouteViaInterface(int cmd, int flags, std::string ifname, IPRange range)
    {
      int if_idx = if_nametoindex(ifname.c_str());
      if (range.IsV4())
      {
        const auto maybe = Net().GetInterfaceAddr(ifname);
        if (not maybe)
          throw std::runtime_error{"we dont have our own network interface?"};

        const _inet_addr gateway{maybe->asIPv4()};

        const _inet_addr addr{
            net::TruncateV6(range.addr), bits::count_bits(net::TruncateV6(range.netmask_bits))};

        Route(cmd, flags, addr, gateway, GatewayMode::eUpperDefault, if_idx);
      }
      else
      {
        const auto maybe = Net().GetInterfaceIPv6Address(ifname);
        if (not maybe)
          throw std::runtime_error{"we dont have our own network interface?"};
        const _inet_addr gateway{*maybe, 128};
        const _inet_addr addr{range.addr, bits::count_bits(range.netmask_bits)};
        Route(cmd, flags, addr, gateway, GatewayMode::eUpperDefault, if_idx);
      }
    }

    void
    Route(int cmd, int flags, IPVariant_t ip, IPVariant_t gateway)
    {
      // do bullshit double std::visit because lol variants
      std::visit(
          [gateway, cmd, flags, this](auto&& ip) {
            const _inet_addr toAddr{ip};
            std::visit(
                [toAddr, cmd, flags, this](auto&& gateway) {
                  const _inet_addr gwAddr{gateway};
                  Route(cmd, flags, toAddr, gwAddr, GatewayMode::eFirstHop, 0);
                },
                gateway);
          },
          ip);
    }

   public:
    LinuxRouteManager() : fd{socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)}
    {
      if (fd == -1)
        throw std::runtime_error{"failed to make netlink socket"};
    }

    ~LinuxRouteManager()
    {
      close(fd);
    }

    void
    AddRoute(IPVariant_t ip, IPVariant_t gateway) override
    {
      Route(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, ip, gateway);
    }

    void
    DelRoute(IPVariant_t ip, IPVariant_t gateway) override
    {
      Route(RTM_DELROUTE, 0, ip, gateway);
    }

    void
    AddDefaultRouteViaInterface(std::string ifname) override
    {
      DefaultRouteViaInterface(ifname, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL);
    }

    void
    DelDefaultRouteViaInterface(std::string ifname) override
    {
      DefaultRouteViaInterface(ifname, RTM_DELROUTE, 0);
    }

    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, vpn.IfName(), range);
    }

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(RTM_DELROUTE, 0, vpn.IfName(), range);
    }

    std::vector<IPVariant_t>
    GetGatewaysNotOnInterface(std::string ifname) override
    {
      std::vector<IPVariant_t> gateways{};
      std::ifstream inf{"/proc/net/route"};
      for (std::string line; std::getline(inf, line);)
      {
        const auto parts = split(line, "\t");
        if (parts[1].find_first_not_of('0') == std::string::npos and parts[0] != ifname)
        {
          const auto& ip = parts[2];
          if ((ip.size() == sizeof(uint32_t) * 2) and oxenc::is_hex(ip))
          {
            huint32_t x{};
            oxenc::from_hex(ip.begin(), ip.end(), reinterpret_cast<char*>(&x.h));
            gateways.emplace_back(x);
          }
        }
      }
      return gateways;
    }

    void
    AddBlackhole() override
    {
      Blackhole(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET);
      Blackhole(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET6);
    }

    void
    DelBlackhole() override
    {
      Blackhole(RTM_DELROUTE, 0, AF_INET);
      Blackhole(RTM_DELROUTE, 0, AF_INET6);
    }
  };

  class LinuxPlatform : public Platform
  {
    LinuxRouteManager _routeManager{};

   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter*) override
    {
      return std::make_shared<LinuxInterface>(std::move(info));
    };

    IRouteManager&
    RouteManager() override
    {
      return _routeManager;
    }
  };

}  // namespace llarp::vpn

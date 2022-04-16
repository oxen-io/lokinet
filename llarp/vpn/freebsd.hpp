#pragma once

#include <llarp/ev/vpn.hpp>

#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "ioctl.hpp"

namespace llarp::vpn
{
  class FreebsdInterface : public NetworkInterface
  {
    const InterfaceInfo m_Info;
    const int m_TunFD;
    const char* const m_DevName;

   public:
    FreebsdInterface(InterfaceInfo info)
        : m_Info{std::move(info)}
        , m_TunFD{::open("/dev/tun", O_RDWR)}
        , m_DevName{::fdevname(m_TunFD)}
    {
      int mode{};
      if (m_TunFD == -1)
        throw std::runtime_error{stringify("did not open /dev/tun: ", strerror(errno))};
      if (m_DevName == nullptr)
        throw std::runtime_error{stringify("could not determine devname ", strerror(errno))};

      try
      {
        // point to point
        mode = IFF_POINTOPOINT;
        IOCTL::attempt(m_TunFD, TUNSIFMODE, &mode);
      }
      catch (std::exception& ex)
      {
        throw std::runtime_error{stringify("cannot set ioctl TUNSIFMODE: ", ex.what())};
      }

      try
      {
        // packet info
        mode = TUNSIFHEAD;
        IOCTL::attempt(m_TunFD, TUNSLMODE, &mode);
      }
      catch (std::exception& ex)
      {
        throw std::runtime_error{stringify("cannot set ioctl TUNSLMODE: ", ex.what())};
      }

      try
      {
        // owned pid
        IOCTL::attempt(m_TunFD, TUNSIFPID);
      }
      catch (std::exception& ex)
      {
        throw std::runtime_error{stringify("cannot set ioctl TUNSIFPID: ", ex.what())};
      }

      try
      {
        // non blocking
        mode = 1;
        IOCTL::attempt(m_TunFD, FIONBIO, &mode);
      }
      catch (std::exception& ex)
      {
        throw std::runtime_error{stringify("cannot set ioctl FIONBIO: ", ex.what())};
      }

      ifreq req{};

      // lambda that clears the entire ifreq
      auto req_clear = [&req]() { std::memset(&req, 0, sizeof(req)); };

      // lambda to set device name in ifreq
      auto req_set_name = [&req](const char* name) {
        std::copy_n(name, strnlen(name, sizeof(req.ifr_name)), req.ifr_name);
        req.ifr_index = if_nameindex(name);
      };
      // lambda to set address range in ifreq for ipv4
      auto req_set_addr_range_v4 = [&req](const auto& range) {
        auto* addr = reinterpret_cast<sockaddr_in*>(&req.ifr_addr);
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = ToNet(net::TruncateV4(range.addr)).n;
      };
      // lambda to set address range in ifreq for ipv6
      auto req_set_addr_range_v6 = [&req](const auto& range) {
        auto* addr = reinterpret_cast<sockaddr_in6*>(&req.ifr_addr);
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = HUIntToIn6(range.addr);
      };

      for (const auto& addr : m_Info.addrs)
      {
        try
        {
          IOCTL i{addr.fam};

          // set up request
          req_clear();
          req_set_name(m_DevName);

          switch (addr.fam)
          {
            case AF_INET:
              req_set_addr_range_v4(addr.range);
              break;
            case AF_INET6:
              req_set_addr_range_v6(addr.range);
              break;
            default:
              throw std::logic_error{
                  stringify("cannot use address of invalid address family: ", addr.fam)};
          }
          i.ioct(SIOCSIFADDR, &req);
        }
        catch (std::exception& ex)
        {
          throw std::runtime_error{
              stringify("cannot add interface address ", addr.range, ": ", ex.what())};
        }
      }

      try
      {
        req_clear();
        req_set_name(m_DevName);
        IOCTL::attempt(m_TUNFD, SIOCGIFFLAGS, &req);
        req.ifr_flags |= IFF_UP;
        IOCTL::attempt(m_TUNFD, SIOCSIFFLAGS, &req);
      }
      catch (std::exception& ex)
      {
        throw std::runtime_error{stringify("cannot put interface up: ", ex.what())};
      }
    }

    ~FreebsdInterface() override
    {
      ::close(m_TunFD);
    }

    std::string
    IfName() const override
    {
      return m_DevName;
    };

    int
    PollFD() const override
    {
      return m_TunFD;
    }

    net::IPPacket
    ReadNextPacket() override
    {
      int pktinfo;
      net::IPPacket pkt{};
      const iovec vecs[2] = {{&pktinfo, sizeof(pktinfo)}, {&pkt.buf[0], sizeof(pkt.buf)}};
      if (auto n = ::readv(m_TunFD, &vecs, 2); n < sizeof(pktinfo))
      {
        pkt.sz = n - sizeof(pktinfo);
      }
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      int pktinfo = pkt.AddressFamily();
      const iovec vecs[2] = {{&pktinfo, sizeof(pktinfo)}, {&pkt.buf[0], pkt.sz}};
      return ::writev(m_TunFD, &vecs, 2) == (pkt.sz + sizeof(pktinfo));
    }
  };

  class FreebsdPlatform : public Platform, public IRouteManager
  {
    const int m_Route4FD;
    const int m_Route6FD;

   public:
    FreebsdPlatform(llarp::Context* ctx)
        : Platform{ctx}
        , m_Route4FD{::socket(PF_ROUTE, SOCK_RAW, AF_INET)}
        , m_Route6FD{::socket(PF_ROUTE, SOCK_RAW, AF_INET6)}
    {
      if (m_Route4FD == -1)
        throw std::runtime_error{
            stringify("cannot make routing socket for AF_INET: ", strerror(errno))};

      if (m_Route6FD == -1)
      {
        const auto err = errno;
        ::close(m_Route4FD);
        throw std::runtime_error{
            stringify("cannot make routing socket for AF_INET6: ", strerror(err))};
      }
    }

    ~FreebsdPlatform() override
    {
      ::close(m_Route4FD);
      ::close(m_Route6FD);
    }
  };
  namespace freebsd
  {
    using VPNPlatform = FreebsdPlatform;
  }

}  // namespace llarp::vpn

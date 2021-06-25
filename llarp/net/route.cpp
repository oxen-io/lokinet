#include "route.hpp"

#ifdef __linux__
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#ifndef ANDROID
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#endif
#include "net.hpp"
#include <exception>
#include <charconv>
#endif
#ifdef __APPLE__
#include "net.hpp"
#include <llarp/util/str.hpp>
#endif
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <cstring>
#include <locale>
#include <codecvt>
#include "net_int.hpp"
#include "ip.hpp"
#endif

#include <sstream>
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/str.hpp>

namespace llarp::net
{
#ifndef __linux__
  void
  Execute(std::string cmd)
  {
    LogInfo(cmd);
    system(cmd.c_str());
  }
#endif

#ifdef __linux__
#ifndef ANDROID

  enum class GatewayMode
  {
    eFirstHop,
    eLowerDefault,
    eUpperDefault
  };

  struct NLSocket
  {
    NLSocket() : fd(socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
    {
      if (fd == -1)
        throw std::runtime_error("failed to make netlink socket");
    }

    ~NLSocket()
    {
      if (fd != -1)
        close(fd);
    }

    const int fd;
  };

  /* Helper structure for ip address data and attributes */
  typedef struct
  {
    unsigned char family;
    unsigned char bitlen;
    unsigned char data[sizeof(struct in6_addr)];
  } _inet_addr;

  /* */

#define NLMSG_TAIL(nmsg) ((struct rtattr*)(((intptr_t)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

  /* Add new data to rtattr */
  int
  rtattr_add(struct nlmsghdr* n, unsigned int maxlen, int type, const void* data, int alen)
  {
    int len = RTA_LENGTH(alen);
    struct rtattr* rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen)
    {
      fprintf(stderr, "rtattr_add error: message exceeded bound of %d\n", maxlen);
      return -1;
    }

    rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = len;

    if (alen)
    {
      memcpy(RTA_DATA(rta), data, alen);
    }

    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

    return 0;
  }

  struct nl_req
  {
    struct nlmsghdr n;
    struct rtmsg r;
    char buf[4096];
  };

  /// add/remove a route blackhole
  int
  do_blackhole(int sock, int cmd, int flags, int af)
  {
    nl_req nl_request{};
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
      rtattr_add(&nl_request.n, sizeof(nl_request), /*RTA_NEWDST*/ RTA_DST, &addr, sizeof(addr));
    }
    else
    {
      uint128_t addr{};
      rtattr_add(&nl_request.n, sizeof(nl_request), /*RTA_NEWDST*/ RTA_DST, &addr, sizeof(addr));
    }
    return send(sock, &nl_request, sizeof(nl_request), 0);
  }

  int
  do_route(
      int sock,
      int cmd,
      int flags,
      const _inet_addr* dst,
      const _inet_addr* gw,
      GatewayMode mode,
      int if_idx)
  {
    nl_req nl_request{};

    /* Initialize request structure */
    nl_request.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nl_request.n.nlmsg_flags = NLM_F_REQUEST | flags;
    nl_request.n.nlmsg_type = cmd;
    nl_request.n.nlmsg_pid = getpid();
    nl_request.r.rtm_family = dst->family;
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

    nl_request.r.rtm_family = dst->family;
    nl_request.r.rtm_dst_len = dst->bitlen;
    nl_request.r.rtm_scope = 0;

    /* Set gateway */
    if (gw->bitlen != 0 and dst->family == AF_INET)
    {
      rtattr_add(&nl_request.n, sizeof(nl_request), RTA_GATEWAY, &gw->data, gw->bitlen / 8);
    }
    nl_request.r.rtm_family = gw->family;
    if (mode == GatewayMode::eFirstHop)
    {
      rtattr_add(
          &nl_request.n, sizeof(nl_request), /*RTA_NEWDST*/ RTA_DST, &dst->data, dst->bitlen / 8);
      /* Set interface */
      rtattr_add(&nl_request.n, sizeof(nl_request), RTA_OIF, &if_idx, sizeof(int));
    }
    if (mode == GatewayMode::eUpperDefault)
    {
      if (dst->family == AF_INET)
      {
        rtattr_add(
            &nl_request.n,
            sizeof(nl_request),
            /*RTA_NEWDST*/ RTA_DST,
            &dst->data,
            sizeof(uint32_t));
      }
      else
      {
        rtattr_add(&nl_request.n, sizeof(nl_request), RTA_OIF, &if_idx, sizeof(int));
        rtattr_add(
            &nl_request.n,
            sizeof(nl_request),
            /*RTA_NEWDST*/ RTA_DST,
            &dst->data,
            sizeof(in6_addr));
      }
    }
    /* Send message to the netlink */
    return send(sock, &nl_request, sizeof(nl_request), 0);
  }

  int
  read_addr(const char* addr, _inet_addr* res, int bitlen = 32)
  {
    if (strchr(addr, ':'))
    {
      res->family = AF_INET6;
      res->bitlen = bitlen;
    }
    else
    {
      res->family = AF_INET;
      res->bitlen = bitlen;
    }
    return inet_pton(res->family, addr, res->data);
  }

#endif
#endif

#ifdef _WIN32

  std::wstring
  get_win_sys_path()
  {
    wchar_t win_sys_path[MAX_PATH] = {0};
    const wchar_t* default_sys_path = L"C:\\Windows\\system32";

    if (!GetSystemDirectoryW(win_sys_path, _countof(win_sys_path)))
    {
      wcsncpy(win_sys_path, default_sys_path, _countof(win_sys_path));
      win_sys_path[_countof(win_sys_path) - 1] = L'\0';
    }
    return win_sys_path;
  }

  std::string
  RouteCommand()
  {
    std::wstring wcmd = get_win_sys_path() + L"\\route.exe";

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    return converter.to_bytes(wcmd);
  }

  std::string
  NetshCommand()
  {
    std::wstring wcmd = get_win_sys_path() + L"\\netsh.exe";

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    return converter.to_bytes(wcmd);
  }

  template <typename Visit>
  void
  ForEachWIN32Interface(Visit visit)
  {
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
    MIB_IPFORWARDTABLE* pIpForwardTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(sizeof(MIB_IPFORWARDTABLE));
    if (pIpForwardTable == nullptr)
      return;

    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
    {
      FREE(pIpForwardTable);
      pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(dwSize);
      if (pIpForwardTable == nullptr)
      {
        return;
      }
    }

    if ((dwRetVal = GetIpForwardTable(pIpForwardTable, &dwSize, 0)) == NO_ERROR)
    {
      for (int i = 0; i < (int)pIpForwardTable->dwNumEntries; i++)
      {
        visit(&pIpForwardTable->table[i]);
      }
    }
    FREE(pIpForwardTable);
#undef MALLOC
#undef FREE
  }

  std::optional<int>
  GetInterfaceIndex(huint32_t ip)
  {
    std::optional<int> ret = std::nullopt;
    ForEachWIN32Interface([&ret, n = ToNet(ip)](auto* iface) {
      if (ret.has_value())
        return;
      if (iface->dwForwardNextHop == n.n)
      {
        ret = iface->dwForwardIfIndex;
      }
    });
    return ret;
  }

#endif

  void
  AddRoute(std::string ip, std::string gateway)
  {
    LogInfo("Add route: ", ip, " via ", gateway);
#ifdef __linux__
#ifndef ANDROID
    NLSocket sock;
    int if_idx = 0;
    _inet_addr to_addr{};
    _inet_addr gw_addr{};
    int nl_cmd = RTM_NEWROUTE;
    int nl_flags = NLM_F_CREATE | NLM_F_EXCL;
    read_addr(gateway.c_str(), &gw_addr);
    read_addr(ip.c_str(), &to_addr);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eFirstHop, if_idx);
#endif
#else
    std::stringstream ss;
#if _WIN32
    ss << RouteCommand() << " ADD " << ip << " MASK 255.255.255.255 " << gateway << " METRIC 2";
#elif __APPLE__
    ss << "/sbin/route -n add -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
    Execute(ss.str());
#endif
  }

  void
  DelRoute(std::string ip, std::string gateway)
  {
    LogInfo("Delete route: ", ip, " via ", gateway);
#ifdef __linux__
#ifndef ANDROID
    NLSocket sock;
    int if_idx = 0;
    _inet_addr to_addr{};
    _inet_addr gw_addr{};
    int nl_cmd = RTM_DELROUTE;
    int nl_flags = 0;
    read_addr(gateway.c_str(), &gw_addr);
    read_addr(ip.c_str(), &to_addr);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eFirstHop, if_idx);
#endif
#else
    std::stringstream ss;
#if _WIN32
    ss << RouteCommand() << " DELETE " << ip << " MASK 255.255.255.255 " << gateway << " METRIC 2";
#elif __APPLE__
    ss << "/sbin/route -n delete -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
    Execute(ss.str());
#endif
  }

  void
  AddBlackhole()
  {
    LogInfo("adding route blackhole to drop all traffic");
#if __linux__
#ifndef ANDROID
    NLSocket sock;
    do_blackhole(sock.fd, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET);
    do_blackhole(sock.fd, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET6);
#endif
#endif
  }

  void
  DelBlackhole()
  {
    LogInfo("remove route blackhole");
#if __linux__
#ifndef ANDROID
    NLSocket sock;
    do_blackhole(sock.fd, RTM_DELROUTE, 0, AF_INET);
    do_blackhole(sock.fd, RTM_DELROUTE, 0, AF_INET6);
#endif
#endif
  }

  void
  AddDefaultRouteViaInterface(std::string ifname)
  {
    LogInfo("Add default route via ", ifname);
#ifdef __linux__
#ifndef ANDROID
    NLSocket sock;
    int if_idx = if_nametoindex(ifname.c_str());
    _inet_addr to_addr{};
    _inet_addr gw_addr{};
    const auto maybe = GetInterfaceAddr(ifname);
    if (not maybe.has_value())
      throw std::runtime_error("we dont have our own net interface?");
    int nl_cmd = RTM_NEWROUTE;
    int nl_flags = NLM_F_CREATE | NLM_F_EXCL;
    const auto host = maybe->asIPv4().ToString();
    read_addr(host.c_str(), &gw_addr);
    read_addr("0.0.0.0", &to_addr, 1);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eLowerDefault, if_idx);
    read_addr("128.0.0.0", &to_addr, 1);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
    const auto maybeInt = GetInterfaceIPv6Address(ifname);
    if (maybeInt.has_value())
    {
      const auto host = maybeInt->ToString();
      LogInfo("add v6 route via ", host);
      read_addr(host.c_str(), &gw_addr, 128);
      read_addr("::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("4000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("8000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("c000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
    }
#endif
#elif _WIN32
    // poke hole for loopback bacause god is dead on windows
    Execute(RouteCommand() + " ADD 127.0.0.0 MASK 255.0.0.0 0.0.0.0");

    huint32_t ip{};
    ip.FromString(ifname);
    const auto ipv6 = net::ExpandV4Lan(ip);

    Execute(RouteCommand() + " ADD ::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " ADD 4000::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " ADD 8000::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " ADD c000::/2 " + ipv6.ToString());
    ifname.back()++;
    Execute(RouteCommand() + " ADD 0.0.0.0 MASK 128.0.0.0 " + ifname);
    Execute(RouteCommand() + " ADD 128.0.0.0 MASK 128.0.0.0 " + ifname);

#elif __APPLE__
    Execute("/sbin/route -n add -cloning -net 0.0.0.0 -netmask 128.0.0.0 -interface " + ifname);
    Execute("/sbin/route -n add -cloning -net 128.0.0.0 -netmask 128.0.0.0 -interface " + ifname);

    Execute("/sbin/route -n add -inet6 -net ::/2 -interface " + ifname);
    Execute("/sbin/route -n add -inet6 -net 4000::/2 -interface " + ifname);
    Execute("/sbin/route -n add -inet6 -net 8000::/2 -interface " + ifname);
    Execute("/sbin/route -n add -inet6 -net c000::/2 -interface " + ifname);
#else
#error unsupported platform
#endif
  }

  void
  DelDefaultRouteViaInterface(std::string ifname)
  {
    LogInfo("Remove default route via ", ifname);
#ifdef __linux__
#ifndef ANDROID
    NLSocket sock;
    int if_idx = if_nametoindex(ifname.c_str());
    _inet_addr to_addr{};
    _inet_addr gw_addr{};
    const auto maybe = GetInterfaceAddr(ifname);

    if (not maybe.has_value())
      throw std::runtime_error("we dont have our own net interface?");
    int nl_cmd = RTM_DELROUTE;
    int nl_flags = 0;
    const auto host = maybe->asIPv4().ToString();
    read_addr(host.c_str(), &gw_addr);
    read_addr("0.0.0.0", &to_addr, 1);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eLowerDefault, if_idx);
    read_addr("128.0.0.0", &to_addr, 1);
    do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);

    const auto maybeInt = GetInterfaceIPv6Address(ifname);
    if (maybeInt.has_value())
    {
      const auto host = maybeInt->ToString();
      LogInfo("del v6 route via ", host);
      read_addr(host.c_str(), &gw_addr, 128);
      read_addr("::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("4000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("8000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
      read_addr("c000::", &to_addr, 2);
      do_route(sock.fd, nl_cmd, nl_flags, &to_addr, &gw_addr, GatewayMode::eUpperDefault, if_idx);
    }
#endif
#elif _WIN32
    huint32_t ip{};
    ip.FromString(ifname);
    const auto ipv6 = net::ExpandV4Lan(ip);

    Execute(RouteCommand() + " DELETE ::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " DELETE 4000::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " DELETE 8000::/2 " + ipv6.ToString());
    Execute(RouteCommand() + " DELETE c000::/2 " + ipv6.ToString());
    ifname.back()++;
    Execute(RouteCommand() + " DELETE 0.0.0.0 MASK 128.0.0.0 " + ifname);
    Execute(RouteCommand() + " DELETE 128.0.0.0 MASK 128.0.0.0 " + ifname);
    Execute(RouteCommand() + " DELETE 127.0.0.0 MASK 255.0.0.0 0.0.0.0");
#elif __APPLE__
    Execute("/sbin/route -n delete -cloning -net 0.0.0.0 -netmask 128.0.0.0 -interface " + ifname);
    Execute(
        "/sbin/route -n delete -cloning -net 128.0.0.0 -netmask 128.0.0.0 -interface " + ifname);

    Execute("/sbin/route -n delete -inet6 -net ::/2 -interface " + ifname);
    Execute("/sbin/route -n delete -inet6 -net 4000::/2 -interface " + ifname);
    Execute("/sbin/route -n delete -inet6 -net 8000::/2 -interface " + ifname);
    Execute("/sbin/route -n delete -inet6 -net c000::/2 -interface " + ifname);

#else
#error unsupported platform
#endif
  }

  std::vector<std::string>
  GetGatewaysNotOnInterface(std::string ifname)
  {
    std::vector<std::string> gateways;
#ifdef __linux__
#ifdef ANDROID
#else
    std::ifstream inf("/proc/net/route");
    for (std::string line; std::getline(inf, line);)
    {
      const auto parts = split(line, '\t');
      if (parts[1].find_first_not_of('0') == std::string::npos and parts[0] != ifname)
      {
        const auto& ip = parts[2];
        if ((ip.size() == sizeof(uint32_t) * 2) and oxenmq::is_hex(ip))
        {
          huint32_t x{};
          oxenmq::from_hex(ip.begin(), ip.end(), reinterpret_cast<char*>(&x.h));
          gateways.emplace_back(x.ToString());
        }
      }
    }
#endif
    return gateways;
#elif _WIN32
    ForEachWIN32Interface([&](auto w32interface) {
      struct in_addr gateway, interface_addr;
      gateway.S_un.S_addr = (u_long)w32interface->dwForwardDest;
      interface_addr.S_un.S_addr = (u_long)w32interface->dwForwardNextHop;
      std::string interface_name{inet_ntoa(interface_addr)};
      if ((!gateway.S_un.S_addr) and interface_name != ifname)
      {
        llarp::LogTrace(
            "Win32 find gateway: Adding gateway (", interface_name, ") to list of gateways.");
        gateways.push_back(std::move(interface_name));
      }
    });
    return gateways;
#elif __APPLE__
    LogDebug("get gateways not on ", ifname);
    // mac os is so godawful man
    FILE* p = popen("/usr/sbin/netstat -rn -f inet", "r");
    if (p == nullptr)
    {
      return gateways;
    }
    char* line = nullptr;
    size_t len = 0;
    ssize_t read = 0;
    while ((read = getline(&line, &len, p)) != -1)
    {
      const std::string line_str(line, len);
      const auto parts = llarp::split_any(line_str, " "sv, true);
      if (parts[0] == "default" and parts[3] != ifname)
      {
        gateways.emplace_back(parts[1]);
      }
    }
    pclose(p);
    return gateways;
#else
#error unsupported platform
#endif
  }

}  // namespace llarp::net

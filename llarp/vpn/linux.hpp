#pragma once

#include "common.hpp"
#include "platform.hpp"

#include <llarp.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/str.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <oxenc/endian.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace llarp::vpn
{
    static auto logcat = log::Cat("vpn.linux");

    inline constexpr std::array<ipv6, 4> if_ipv6_addrs{ipv6{}, ipv6{0x4000}, ipv6{0x8000}, ipv6{0xc000}};

    struct in6_ifreq
    {
        in6_addr addr;
        uint32_t prefixlen;
        unsigned int ifindex;
    };

    struct call_on_destroy
    {
        std::function<void()> f;
        ~call_on_destroy()
        {
            if (f)
                f();
        }
        void disarm() { f = nullptr; }
    };

    // Send a netlink request and wait for the response.  Returns an error string on error, nullopt
    // on success.  Must have set NLM_F_ACK on the request header flags!
    template <typename NLRequestT>
    std::optional<std::string> nl_submit(int nlfd, const NLRequestT& req)
    {
        if (-1 == send(nlfd, &req, req.header.nlmsg_len, 0))
            return strerror(errno);

        char resp_buf[4096];
        int resp_len = recv(nlfd, resp_buf, sizeof(resp_buf), 0);
        auto* resp = reinterpret_cast<nlmsghdr*>(resp_buf);
        if (!NLMSG_OK(resp, resp_len) || resp->nlmsg_type != NLMSG_ERROR)
            return "Invalid netlink response"s;
        else if (auto* nlerr = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(resp_buf)); nlerr->error < 0)
            return strerror(-nlerr->error);
        return std::nullopt;
    }

    class LinuxInterface : public NetworkInterface
    {
        const int _fd;

      public:
        LinuxInterface(InterfaceInfo info) : NetworkInterface{std::move(info)}, _fd{::open("/dev/net/tun", O_RDWR)}
        {
            if (_fd == -1)
                throw std::runtime_error("cannot open /dev/net/tun {}"_format(strerror(errno)));

            call_on_destroy fd_abort{[fd = _fd] { close(fd); }};

            if (fcntl(_fd, F_SETFL, O_NONBLOCK) == -1)
                throw std::runtime_error{
                    "Failed to set `O_NONBLOCK` on Linux interface FD: {}"_format(strerror(errno))};

            ifreq ifr{};
            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
            std::memcpy(ifr.ifr_name, _info.ifname.c_str(), std::min<size_t>(_info.ifname.size(), IFNAMSIZ - 1));

            if (::ioctl(_fd, TUNSETIFF, &ifr) == -1)
                throw std::runtime_error{"Cannot set TUN interface name: {}"_format(strerror(errno))};

            // The ioctl above could have changed the tun device on us:
            _info.ifname = ifr.ifr_name;
            _info.index = if_nametoindex(_info.ifname.c_str());

            int nlfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
            if (nlfd == -1)
                throw std::runtime_error{"netlink failed: {}"_format(strerror(errno))};
            call_on_destroy nlfd_cleanup([nlfd] { close(nlfd); });

            if (_info.addrs.empty())
                throw std::runtime_error{"Cannot set up a TUN interface with no addresses!"};

            std::list<std::string> addr_strings;
            // Add addresses to the tun interface:
            for (const auto& ifaddr : _info.addrs)
            {
                struct
                {
                    nlmsghdr header;
                    ifaddrmsg content;
                    char buf[256];
                } request{};
                size_t buf_avail = sizeof(request.buf);

                request.header.nlmsg_len = NLMSG_LENGTH(sizeof request.content);
                request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE | NLM_F_ACK;
                request.header.nlmsg_type = RTM_NEWADDR;
                request.content.ifa_index = _info.index;

                if (auto* n4 = std::get_if<ipv4_net>(&ifaddr))
                {
                    addr_strings.push_back(n4->to_string());
                    request.content.ifa_family = AF_INET;
                    request.content.ifa_prefixlen = n4->mask;
                    auto* req_attr = IFA_RTA(&request.content);
                    req_attr->rta_type = IFA_LOCAL;
                    req_attr->rta_len = RTA_LENGTH(sizeof(in_addr));
                    request.header.nlmsg_len += req_attr->rta_len;
                    oxenc::write_host_as_big(n4->ip.addr, RTA_DATA(req_attr));

                    req_attr = RTA_NEXT(req_attr, buf_avail);
                    req_attr->rta_type = IFA_ADDRESS;
                    req_attr->rta_len = RTA_LENGTH(sizeof(in_addr));
                    request.header.nlmsg_len += req_attr->rta_len;
                    oxenc::write_host_as_big(n4->ip.addr, RTA_DATA(req_attr));
                }
                else
                {
                    auto& n6 = std::get<ipv6_net>(ifaddr);
                    addr_strings.push_back(n6.to_string());
                    request.content.ifa_family = AF_INET6;
                    request.content.ifa_prefixlen = n6.mask;
                    auto* req_attr = IFA_RTA(&request.content);
                    req_attr->rta_type = IFA_LOCAL;
                    req_attr->rta_len = RTA_LENGTH(sizeof(in6_addr));
                    request.header.nlmsg_len += req_attr->rta_len;
                    char* addr_data = static_cast<char*>(RTA_DATA(req_attr));
                    oxenc::write_host_as_big(n6.ip.hi, addr_data);
                    oxenc::write_host_as_big(n6.ip.lo, addr_data + 8);

                    req_attr = RTA_NEXT(req_attr, buf_avail);
                    req_attr->rta_type = IFA_ADDRESS;
                    req_attr->rta_len = RTA_LENGTH(sizeof(in_addr));
                    request.header.nlmsg_len += req_attr->rta_len;
                    addr_data = static_cast<char*>(RTA_DATA(req_attr));
                    oxenc::write_host_as_big(n6.ip.hi, addr_data);
                    oxenc::write_host_as_big(n6.ip.lo, addr_data + 8);
                }

                if (auto err = nl_submit(nlfd, request))
                    throw std::runtime_error{
                        "Failed to add address {} to {}: {}"_format(addr_strings.back(), _info.ifname, *err)};
            }

            // Bring up the tun device:
            {
                struct
                {
                    nlmsghdr header;
                    ifinfomsg content;
                } request{};
                request.header.nlmsg_len = NLMSG_LENGTH(sizeof request.content);
                request.header.nlmsg_flags = NLM_F_REQUEST;
                request.header.nlmsg_type = RTM_NEWLINK;
                request.content.ifi_index = static_cast<int>(_info.index);
                request.content.ifi_flags = IFF_UP;
                request.content.ifi_change = 1;

                if (auto err = nl_submit(nlfd, request))
                    throw std::runtime_error{"Failed to bring up tun device {}: {}"_format(_info.ifname, *err)};
            }

            fd_abort.disarm();
            log::info(logcat, "TUN device {} now up with address(es): {}", _info.ifname, fmt::join(addr_strings, ", "));
        }

        ~LinuxInterface() override { ::close(_fd); }

        int PollFD() const override { return _fd; }

        IPPacket read_next_packet() override
        {
            std::vector<uint8_t> buf;
            buf.resize(MAX_PACKET_SIZE);
            const auto sz = read(_fd, buf.data(), buf.capacity());
            // log::trace(logcat, "{} bytes read from fd {} (err?:{})", sz, _fd, strerror(errno));
            if (sz < 0)
            {
                if (errno == EAGAIN or errno == EWOULDBLOCK)
                {
                    errno = 0;
                    return IPPacket{};
                }
                throw std::error_code{errno, std::system_category()};
            }

            buf.resize(sz);
            return IPPacket{std::move(buf)};
        }

        bool write_packet(IPPacket pkt) override
        {
            const auto sz = write(_fd, pkt.data(), pkt.size());
            // log::trace(logcat, "{} bytes written to fd {} (err?:{})", sz, _fd, strerror(errno));
            if (sz <= 0)
                return false;
            return sz == static_cast<ssize_t>(pkt.size());
        }
    };

    class LinuxRouteManager : public AbstractRouteManager
    {
        const int fd;

        enum class GatewayMode
        {
            FirstHop,  // Selector for a first-hop route, i.e. for route poking
            Default,   // Selector for default routing, i.e. for global exit traffic routing
        };

        struct NLRequest
        {
            nlmsghdr n;
            rtmsg r;
            char buf[4096];

            void AddData(int type, const void* data, int alen)
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

        void make_blackhole(int cmd, int flags, int af)
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
                ipv4 addr{};
                nl_request.AddData(RTA_DST, &addr.addr, sizeof(addr.addr));
            }
            else
            {
                std::array<uint64_t, 2> addr{};
                nl_request.AddData(RTA_DST, &addr, sizeof(addr));
            }
            send(fd, &nl_request, sizeof(nl_request), 0);
        }

        NLRequest init_route_cmd(int cmd, int flags)
        {
            NLRequest nl_request{};
            /* Initialize request structure */
            nl_request.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
            nl_request.n.nlmsg_flags = NLM_F_REQUEST | flags;
            nl_request.n.nlmsg_type = cmd;
            nl_request.n.nlmsg_pid = getpid();
            nl_request.r.rtm_table = RT_TABLE_MAIN;
            nl_request.r.rtm_scope = RT_SCOPE_NOWHERE;

            /* Set additional flags if NOT deleting route */
            if (cmd != RTM_DELROUTE)
            {
                nl_request.r.rtm_protocol = RTPROT_BOOT;
                nl_request.r.rtm_type = RTN_UNICAST;
            }

            return nl_request;
        }

        void route_cmd(int cmd, int flags, const ipv4_range& dst, const ipv4& gw, GatewayMode mode, int if_idx)
        {
            auto nl_request = init_route_cmd(cmd, flags);

            nl_request.r.rtm_family = AF_INET;
            if (if_idx)
                nl_request.r.rtm_scope = RT_SCOPE_LINK;
            nl_request.r.rtm_dst_len = dst.mask;

            char gwbuf[4], ipbuf[4];
            oxenc::write_host_as_big(gw.addr, gwbuf);
            oxenc::write_host_as_big(dst.ip.addr, ipbuf);
            nl_request.AddData(RTA_GATEWAY, gwbuf, sizeof(gwbuf));
            nl_request.AddData(RTA_DST, ipbuf, sizeof(ipbuf));

            if (mode == GatewayMode::FirstHop)
            {
                /* Set interface */
                nl_request.AddData(RTA_OIF, &if_idx, sizeof(if_idx));
            }
            /* Send message to the netlink */
            send(fd, &nl_request, sizeof(nl_request), 0);
        }

        void route_cmd(int cmd, int flags, const ipv6_range& dst, const ipv6& gw, GatewayMode mode, int if_idx)
        {
            auto nl_request = init_route_cmd(cmd, flags);
            nl_request.r.rtm_family = AF_INET6;
            if (if_idx)
                nl_request.r.rtm_scope = RT_SCOPE_UNIVERSE;
            nl_request.r.rtm_dst_len = dst.mask;

            char gwbuf[16], ipbuf[16];
            oxenc::write_host_as_big(gw.hi, gwbuf);
            oxenc::write_host_as_big(gw.lo, gwbuf + 8);
            oxenc::write_host_as_big(dst.ip.hi, ipbuf);
            oxenc::write_host_as_big(dst.ip.lo, ipbuf + 8);
            nl_request.AddData(RTA_GATEWAY, gwbuf, sizeof(gwbuf));
            nl_request.AddData(RTA_DST, ipbuf, sizeof(ipbuf));

            if (mode == GatewayMode::FirstHop)
            {
                /* Set interface */
                nl_request.AddData(RTA_OIF, &if_idx, sizeof(if_idx));
            }
            /* Send message to the netlink */
            send(fd, &nl_request, sizeof(nl_request), 0);
        }

        void route_all_via_interface(NetworkInterface& vpn, int cmd, int flags)
        {
            const auto& info = vpn.interface_info();

            const auto maybe = Net().get_interface_ipv4(info.ifname);
            if (not maybe)
                throw std::runtime_error{"we dont have our own network interface?"};
            auto& tun_ip = *maybe;

            for (const auto& range : {ipv4{0, 0, 0, 0} / 1, ipv4{128, 0, 0, 0} / 1})
                route_cmd(cmd, flags, range, tun_ip, GatewayMode::Default, info.index);

            if (const auto ip6 = Net().get_interface_ipv6(info.ifname))
                for (uint16_t nibble : {0x0000, 0x4000, 0x8000, 0xc000})
                    route_cmd(cmd, flags, ipv6{nibble} / 2, *ip6, GatewayMode::Default, info.index);
        }

        void route_range_via_interface(int cmd, int flags, NetworkInterface& vpn, ipv4_range range)
        {
            const auto& info = vpn.interface_info();
            const auto maybe = Net().get_interface_ipv4(info.ifname);
            if (not maybe)
                throw std::runtime_error{"Unable to add routed IPv4 range: interface has no IPv4 address"};

            route_cmd(cmd, flags, range, *maybe, GatewayMode::Default, info.index);
        }
        void route_range_via_interface(int cmd, int flags, NetworkInterface& vpn, ipv6_range range)
        {
            const auto& info = vpn.interface_info();
            const auto maybe = Net().get_interface_ipv6(info.ifname);
            if (not maybe)
                throw std::runtime_error{"Unable to add routed IPv6 range: interface has no IPv6 address"};

            route_cmd(cmd, flags, range, *maybe, GatewayMode::Default, info.index);
        }

        void route_via_gateway(int cmd, int flags, ipv4_range range, ipv4 gateway)
        {
            route_cmd(cmd, flags, range, gateway, GatewayMode::FirstHop, 0);
        }
        void route_via_gateway(int cmd, int flags, ipv4 dest, ipv4 gateway)
        {
            return route_via_gateway(cmd, flags, dest / 32, gateway);
        }
        void route_via_gateway(int cmd, int flags, ipv6_range range, ipv6 gateway)
        {
            route_cmd(cmd, flags, range, gateway, GatewayMode::FirstHop, 0);
        }
        void route_via_gateway(int cmd, int flags, ipv6 dest, ipv6 gateway)
        {
            return route_via_gateway(cmd, flags, dest / 128, gateway);
        }

      public:
        LinuxRouteManager() : fd{socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)}
        {
            if (fd == -1)
                throw std::runtime_error{"netlink failed: {}"_format(strerror(errno))};
        }

        ~LinuxRouteManager() override { close(fd); }

        void add_route(ipv4 ip, ipv4 gateway) override
        {
            route_via_gateway(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, ip, gateway);
        }
        void add_route(ipv6 ip, ipv6 gateway) override
        {
            route_via_gateway(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, ip, gateway);
        }

        void delete_route(ipv4 ip, ipv4 gateway) override { route_via_gateway(RTM_DELROUTE, 0, ip, gateway); }
        void delete_route(ipv6 ip, ipv6 gateway) override { route_via_gateway(RTM_DELROUTE, 0, ip, gateway); }

        void add_default_route_via_interface(NetworkInterface& vpn) override
        {
            route_all_via_interface(vpn, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL);
        }

        void delete_default_route_via_interface(NetworkInterface& vpn) override
        {
            route_all_via_interface(vpn, RTM_DELROUTE, 0);
        }

        void add_route_via_interface(NetworkInterface& vpn, ipv4_range range) override
        {
            route_range_via_interface(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, vpn, range);
        }
        void add_route_via_interface(NetworkInterface& vpn, ipv6_range range) override
        {
            route_range_via_interface(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, vpn, range);
        }

        void delete_route_via_interface(NetworkInterface& vpn, ipv4_range range) override
        {
            route_range_via_interface(RTM_DELROUTE, 0, vpn, range);
        }

        void delete_route_via_interface(NetworkInterface& vpn, ipv6_range range) override
        {
            route_range_via_interface(RTM_DELROUTE, 0, vpn, range);
        }

        std::vector<quic::Address> get_non_interface_gateways(NetworkInterface& vpn) override
        {
            const auto& ifname = vpn.interface_info().ifname;
            std::vector<quic::Address> gateways{};

            std::ifstream inf{"/proc/net/route"};
            for (std::string line; std::getline(inf, line);)
            {
                const auto parts = split(line, "\t");
                if (parts[1].find_first_not_of('0') == std::string::npos and parts[0] != ifname)
                {
                    const auto& ip = parts[2];
                    if ((ip.size() == sizeof(uint32_t) * 2) and oxenc::is_hex(ip))
                    {
                        std::string buf;
                        oxenc::from_hex(ip.begin(), ip.end(), buf.data());
                        quic::Address addr{buf, 0};
                        gateways.push_back(std::move(addr));
                    }
                }
            }
            return gateways;
        }

        void add_blackhole() override
        {
            make_blackhole(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET);
            make_blackhole(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, AF_INET6);
        }

        void delete_blackhole() override
        {
            make_blackhole(RTM_DELROUTE, 0, AF_INET);
            make_blackhole(RTM_DELROUTE, 0, AF_INET6);
        }
    };

    class LinuxPlatform : public Platform
    {
        LinuxRouteManager _routeManager{};

      public:
        std::shared_ptr<NetworkInterface> obtain_interface(InterfaceInfo info, Router*) override
        {
            return std::make_shared<LinuxInterface>(std::move(info));
        };

        AbstractRouteManager& RouteManager() override { return _routeManager; }
    };

}  // namespace llarp::vpn

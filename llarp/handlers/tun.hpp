#pragma once

#include "tun_base.hpp"

#include <llarp/address/map.hpp>
#include <llarp/dns/server.hpp>
#include <llarp/ev/fd_poller.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/vpn/packet_router.hpp>
#include <llarp/vpn/platform.hpp>

namespace llarp::traffic_type
{
    constexpr uint8_t UDP = 0;
    constexpr uint8_t TCP = 1;
    constexpr uint8_t RAW = 2;
    constexpr uint8_t TUNNELED_QUIC = 3;

    inline constexpr bool is_valid(uint8_t t) { return t >= UDP && t <= TUNNELED_QUIC; }
}  // namespace llarp::traffic_type

namespace llarp::handlers
{
    inline constexpr auto TUN = "tun"sv;
    inline constexpr auto LOKI_RESOLVER = "lokinet"sv;

    class TunEndpoint : public TunEPBase, public dns::Resolver_Base, public std::enable_shared_from_this<TunEndpoint>
    {
      public:
        TunEndpoint(Router& r);
        ~TunEndpoint() override;

      private:
        Router& _router;

        /// dns subsystem for this endpoint
        std::unique_ptr<dns::Server> _dns;

        /// our local ip network
        ipv4_net _local_net;
        IPv4RangeIterator _local_range_iterator{_local_net};

        std::optional<ipv6_net> _local_ipv6_net;
        std::optional<IPv6RangeIterator> _local_ipv6_range_iterator;

        /// Our local Network Address holding our network pubkey
        NetworkAddress _local_netaddr;

        /// list of strict connect addresses for hooks
        // std::vector<IpAddress> _strict_connect_addrs;

        /// use v6?
        bool ipv6_enabled = false;

        std::string _if_name;

        std::shared_ptr<vpn::NetworkInterface> _net_if;
        std::unique_ptr<ev::FDPoller> _poller;

        std::shared_ptr<vpn::PacketRouter> _packet_router;

        std::optional<net::ExitPolicy> _exit_policy = std::nullopt;

        /// a file to load / store the ephemeral address map to
        std::optional<std::filesystem::path> _persisting_addr_file = std::nullopt;
        bool persist_addrs{false};

        /// for raw packet dns
        std::shared_ptr<vpn::PacketIO> _raw_DNS;

      public:
        vpn::NetworkInterface* get_vpn_interface() { return _net_if.get(); }

        std::string_view name() const { return TUN; }

        int rank() const override { return 0; }

        std::string_view resolver_name() const override { return LOKI_RESOLVER; }

        bool maybe_hook_dns(
            const std::shared_ptr<dns::PacketSource>& source,
            const dns::Message& query,
            const quic::Address& to,
            const quic::Address& from) override;

        // Reconfigures DNS servers and restarts libunbound with the new servers.
        void reconfigure_dns(std::vector<quic::Address> servers);

        void configure();

        std::string get_if_name() const;

        // Returns the lokinet tun IPv4 address
        const ipv4& get_ipv4() const;
        // Returns the lokinet tun IPv6 address by pointer, or nullptr if ipv6 is not configured.
        const ipv6* get_ipv6() const;

        // Returns the lokinet tun IPv4 network; the address is set to this tun device's local
        // address (i.e. the .1 address).
        const ipv4_net& get_ipv4_network() const;

        nlohmann::json ExtractStatus() const;

        bool supports_ipv6() const;

        bool should_hook_dns_message(const dns::Message& msg) const;

        bool handle_hooked_dns_message(dns::Message query, std::function<void(dns::Message)> sendreply);

        void tick_tun(std::chrono::milliseconds now);

        bool stop();

        bool is_service_node() const;

        bool is_exit_node() const;

        void setup_dns();

        // INPROGRESS: new API
        // Handles an outbound packet going OUT to the network
        void handle_outbound_packet(IPPacket pkt);

        void rewrite_and_send_packet(IPPacket&& pkt, const ipv4& src, const ipv4& dest);
        void rewrite_and_send_packet(IPPacket&& pkt, const ipv6& src, const ipv6& dest);

        // TESTNET: TODO: new inbound packet handling logic
        void handle_inbound_packet(IPPacket pkt, uint8_t type, NetworkAddress remote) override;

        // Handles an inbound packet coming IN from the network
        // bool handle_inbound_packet(IPPacket pkt, NetworkAddress remote, bool is_exit_session, bool
        // is_outbound_session);

        // Obtains an available IPv4 address from the tun device and associates the given lokinet
        // remote address with it.  If the mapping already exists, this returns the existing IP,
        // otherwise it assigns a new one.  The association persists until unmapped.  Returns the
        // mapped ipv4 address, or nullptr if one could not be assigned.
        std::optional<ipv4> map(const NetworkAddress& remote) override;
        // TODO:
        // std::optional<ipv6> map_address_to_local_ipv6(const NetworkAddress& remote);

        // Removes any mapped IP for the given remote from the tun IP map.
        void unmap(const NetworkAddress& remote) override;

        std::optional<net::ExitPolicy> get_exit_policy() const { return _exit_policy; }

        /// ip packet against any exit policies we have
        /// returns false if this traffic is disallowed by any of those policies
        /// returns true otherwise
        bool is_allowing_traffic(const IPPacket& pkt) const;

        std::pair<std::optional<ipv4>, std::optional<ipv6>> get_mapped_ip(const NetworkAddress& addr);

        const Router& router() const { return _router; }

        Router& router() { return _router; }

        void start_poller() override;

        // Stores assigned IP's for each session in/out of this lokinet instance
        //  - Reserved local addresses are directly pre-loaded from config
        //  - Persisting address map is directly pre-loaded from config
        address_map<ipv4> _local_ipv4_mapping;
        address_map<ipv6> _local_ipv6_mapping;

      private:
        std::optional<ipv4> get_next_local_ipv4();
        std::optional<ipv6> get_next_local_ipv6();

        std::optional<ipv4> obtain_src_for_ipv4_remote(const NetworkAddress& remote);
        std::optional<ipv6> obtain_src_for_ipv6_remote(const NetworkAddress& remote);

        void send_packet_to_net_if(IPPacket pkt);
    };

}  // namespace llarp::handlers

#pragma once

#include "message.hpp"
#include "platform.hpp"

#include <llarp/config/config.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/util/compare_ptr.hpp>

#include <oxen/quic/address.hpp>
#include <oxen/quic/loop.hpp>

#include <set>
#include <utility>

namespace llarp::dns
{
    /// a job handling 1 dns query
    class QueryJob_Base
    {
      protected:
        /// the original dns query
        Message _query;

        /// True if we've sent a reply (including via a call to cancel)
        std::atomic_flag _done = ATOMIC_FLAG_INIT;

      public:
        explicit QueryJob_Base(Message query) : _query{std::move(query)} {}

        virtual ~QueryJob_Base() = default;

        Message& underlying() { return _query; }

        const Message& underlying() const { return _query; }

        /// cancel this operation and inform anyone who cares
        void cancel();

        /// send a raw buffer back to the querier
        virtual void send_reply(std::vector<std::byte> buf) = 0;
    };

    class PacketSource
    {
      public:
        /// stop reading packets and end operation
        virtual ~PacketSource() = default;

        /// return true if traffic with source and dest addresses would cause a
        /// loop in resolution and thus should not be sent to query handlers
        virtual bool would_loop(const quic::Address& to, const quic::Address& from) const = 0;

        /// send UDP payload with src and dst address containing buf on this packet source
        virtual void send_udp(
            const quic::Address& to, const quic::Address& from, std::span<const std::byte> payload) const = 0;

        /// returns the sockaddr we are bound on if applicable
        virtual std::optional<quic::Address> bound_on() const = 0;
    };

    /// non complex implementation of QueryJob_Base for use in things that
    /// only ever called on the mainloop thread
    class QueryJob : public QueryJob_Base, std::enable_shared_from_this<QueryJob>
    {
        std::shared_ptr<PacketSource> src;
        const quic::Address resolver;
        const quic::Address asker;

      public:
        explicit QueryJob(
            std::shared_ptr<PacketSource> source,
            const Message& query,
            const quic::Address& to_,
            const quic::Address& from_)
            : QueryJob_Base{query}, src{std::move(source)}, resolver{to_}, asker{from_}
        {}

        void send_reply(std::vector<std::byte> buf) override { src->send_udp(asker, resolver, buf); }
    };

    /// handler of dns query hooking
    /// intercepts dns for internal processing
    class Resolver_Base
    {
      protected:
        /// return the sorting order for this resolver
        /// lower means it will be tried first
        virtual int rank() const = 0;

      public:
        virtual ~Resolver_Base() = default;

        /// less than via rank
        bool operator<(const Resolver_Base& other) const { return rank() < other.rank(); }

        /// greater than via rank
        bool operator>(const Resolver_Base& other) const { return rank() > other.rank(); }

        /// get local socket address that queries are sent from
        virtual std::optional<quic::Address> get_local_addr() const { return std::nullopt; }

        /// get printable name
        virtual std::string_view resolver_name() const = 0;

        /// reset the resolver state, optionally replace upstream info with new info.  The default
        /// base implementation does nothing.
        virtual void reset_resolver(std::optional<std::vector<quic::Address>> = std::nullopt) {}

        /// cancel all pending requests and cease further operation.  Default operation is a no-op.
        virtual void down() {}

        /// attempt to handle a dns message
        /// returns true if we consumed this query and it should not be processed again
        virtual bool maybe_hook_dns(
            const std::shared_ptr<PacketSource>& source,
            const Message& query,
            const quic::Address& to,
            const quic::Address& from) = 0;
    };

    // Base class for DNS proxy
    class Server
    {
      protected:
        /// add a packet source to this server, does share ownership
        void add_packet_source(std::shared_ptr<PacketSource> resolver);
        /// add a resolver to this packet handler, does share ownership
        void add_resolver(std::shared_ptr<Resolver_Base> resolver);

        /// create the platform dependant dns stuff
        virtual std::shared_ptr<I_Platform> create_platform() const;

      public:
        virtual ~Server() = default;

        explicit Server(quic::Loop& loop, llarp::DnsConfig conf, unsigned int netif_index);

        /// returns all sockaddr we have from all of our PacketSources
        std::vector<quic::Address> bound_packet_source_addrs() const;

        /// returns the first sockaddr we have on our packet sources if we have one
        std::optional<quic::Address> first_bound_packet_source_addr() const;

        /// add a resolver to this packet handler, does not share ownership
        void add_resolver(std::weak_ptr<Resolver_Base> resolver);

        /// add a packet source to this server, does not share ownership
        void add_packet_source(std::weak_ptr<PacketSource> resolver);

        /// create a packet source bound on bindaddr but does not add it
        virtual std::shared_ptr<PacketSource> make_packet_source_on(
            const quic::Address& bindaddr, const llarp::DnsConfig& conf);

        /// sets up all internal binds and such and begins operation
        virtual void start();

        /// stops all operation
        virtual void stop();

        /// reset the internal state
        virtual void reset();

        /// create the default resolver for out config
        virtual std::shared_ptr<Resolver_Base> make_default_resolver();

        std::vector<std::weak_ptr<Resolver_Base>> get_all_resolvers() const;

        /// feed a packet buffer from a packet source.
        /// returns true if we decided to process the packet and consumed it
        /// returns false if we dont want to process the packet
        bool maybe_handle_payload(
            const std::shared_ptr<PacketSource>& pktsource,
            const quic::Address& resolver,
            const quic::Address& from,
            std::span<const std::byte> buf);

        /// set which dns mode we are in.
        /// true for intercepting all queries. false for just .loki and .snode
        void set_dns_mode(bool all_queries);

      protected:
        quic::Loop& _loop;
        llarp::DnsConfig _conf;
        std::shared_ptr<I_Platform> _platform;

      private:
        const unsigned int m_NetIfIndex;
        // TODO FIXME: this ownership model is cursed.
        std::set<std::shared_ptr<Resolver_Base>, ComparePtr<std::shared_ptr<Resolver_Base>>> _owned_resolvers;
        std::set<std::weak_ptr<Resolver_Base>, CompareWeakPtr<Resolver_Base>> _resolvers;

        std::vector<std::shared_ptr<PacketSource>> _owned_packet_sources;
        std::vector<std::weak_ptr<PacketSource>> _packet_sources;
    };

}  // namespace llarp::dns

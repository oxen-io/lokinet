#include "server.hpp"
#include "dns.hpp"
#include <llarp/crypto/crypto.hpp>
#include <array>
#include <utility>
#include <llarp/ev/udp_handle.hpp>
#include <optional>
#include <memory>
#include <unbound.h>
#include <uvw.hpp>

namespace llarp::dns
{
  void
  QueryJob_Base::Cancel() const
  {
    Message reply{m_Query};
    reply.AddServFail();
    SendReply(reply.ToBuffer());
  }

  /// sucks up udp packets from a bound socket and feeds it to a server
  class UDPReader : public PacketSource_Base, public std::enable_shared_from_this<UDPReader>
  {
    Server& m_DNS;
    std::shared_ptr<llarp::UDPHandle> m_udp;
    SockAddr m_LocalAddr;

   public:
    explicit UDPReader(Server& dns, const EventLoop_ptr& loop, llarp::SockAddr bindaddr)
        : m_DNS{dns}
    {
      m_udp = loop->make_udp([&](auto&, SockAddr src, llarp::OwnedBuffer buf) {
        if (src == m_LocalAddr)
          return;
        if (not m_DNS.MaybeHandlePacket(weak_from_this(), m_LocalAddr, src, std::move(buf)))
        {
          LogWarn("did not handle dns packet from ", src, " to ", m_LocalAddr);
        }
      });
      m_udp->listen(bindaddr);
      if (auto maybe_addr = BoundOn())
      {
        m_LocalAddr = *maybe_addr;
      }
      else
        throw std::runtime_error{"cannot find which address our dns socket is bound on"};
    }

    std::optional<SockAddr>
    BoundOn() const override
    {
      return m_udp->LocalAddr();
    }

    bool
    WouldLoop(const SockAddr& to, const SockAddr&) const override
    {
      return to != m_LocalAddr;
    }

    void
    SendTo(const SockAddr& to, const SockAddr&, llarp::OwnedBuffer buf) const override
    {
      m_udp->send(to, std::move(buf));
    }

    void
    Stop() override
    {
      m_udp->close();
    }
  };

  namespace libunbound
  {
    class Resolver;

    class Query : public QueryJob_Base
    {
      std::weak_ptr<Resolver> parent;
      std::weak_ptr<PacketSource_Base> src;
      SockAddr resolverAddr;
      SockAddr askerAddr;

     public:
      explicit Query(
          std::weak_ptr<Resolver> parent_,
          Message query,
          std::weak_ptr<PacketSource_Base> pktsrc,
          SockAddr toaddr,
          SockAddr fromaddr)
          : QueryJob_Base{std::move(query)}
          , parent{parent_}
          , src{pktsrc}
          , resolverAddr{std::move(toaddr)}
          , askerAddr{std::move(fromaddr)}
      {}

      virtual void
      SendReply(llarp::OwnedBuffer replyBuf) const override;
    };

    /// Resolver_Base that uses libunbound
    class Resolver : public Resolver_Base, public std::enable_shared_from_this<Resolver>
    {
      std::shared_ptr<ub_ctx> m_ctx;
      std::weak_ptr<EventLoop> m_Loop;
#ifdef _WIN32
      // windows is dumb so we do ub mainloop in a thread
      std::thread runner;
      std::atomic<bool> running;
#else
      std::shared_ptr<uvw::PollHandle> m_Poller;
#endif

      struct ub_result_deleter
      {
        void
        operator()(ub_result* ptr)
        {
          ::ub_resolve_free(ptr);
        }
      };

      static void
      Callback(void* data, int err, ub_result* _result)
      {
        // take ownership of ub_result
        std::unique_ptr<ub_result, ub_result_deleter> result{_result};
        // take ownership of our query
        std::unique_ptr<Query> query{static_cast<Query*>(data)};

        if (err)
        {
          // some kind of error from upstream
          query->Cancel();
          return;
        }

        // rewrite response
        OwnedBuffer pkt{(const byte_t*)result->answer_packet, (size_t)result->answer_len};
        llarp_buffer_t buf{pkt};
        MessageHeader hdr;
        hdr.Decode(&buf);
        hdr.id = query->Underlying().hdr_id;
        buf.cur = buf.base;
        hdr.Encode(&buf);

        // send reply
        query->SendReply(std::move(pkt));
      }

      void
      SetOpt(std::string key, std::string val)
      {
        ub_ctx_set_option(m_ctx.get(), key.c_str(), val.c_str());
      }

      llarp::DnsConfig m_conf;

     public:
      explicit Resolver(const EventLoop_ptr& loop, llarp::DnsConfig conf)
          : m_ctx{::ub_ctx_create(), ::ub_ctx_delete}, m_Loop{loop}, m_conf{std::move(conf)}
      {
        Up(m_conf);
      }

#ifdef _WIN32
      virtual ~Resolver()
      {
        running = false;
        runner.join();
      }
#else
      virtual ~Resolver() = default;
#endif

      std::string_view
      ResolverName() const override
      {
        return "unbound";
      }

      void
      Up(const llarp::DnsConfig& conf)
      {
        // set libunbound settings
        for (const auto& [k, v] : conf.m_ExtraOpts)
          SetOpt(k, v);

        // add host files
        for (const auto& file : conf.m_hostfiles)
        {
          const auto str = file.u8string();
          if (auto ret = ub_ctx_hosts(m_ctx.get(), str.c_str()))
          {
            throw std::runtime_error{
                fmt::format("Failed to add host file {}: {}", file, ub_strerror(ret))};
          }
        }

        // set up forward dns
        for (const auto& dns : conf.m_upstreamDNS)
        {
          std::stringstream ss;
          auto hoststr = dns.hostString();
          ss << hoststr;

          if (const auto port = dns.getPort(); port != 53)
            ss << "@" << port;

          const auto str = ss.str();
          if (auto err = ub_ctx_set_fwd(m_ctx.get(), str.c_str()))
          {
            throw std::runtime_error{
                fmt::format("cannot use {} as upstream dns: {}", str, ub_strerror(err))};
          }
#ifdef __APPLE__
          // On Apple, we configure a localhost resolver to trampoline requests through the tunnel
          // to the actual upstream (because the network extension itself cannot route through the
          // tunnel using normal sockets but instead we "get" to use Apple's interfaces, hurray).
          if (hoststr == "127.0.0.1")
          {
            // Not at all clear why this is needed but without it we get "send failed: Can't
            // assign requested address" when unbound tries to connect to the localhost address
            // using a source address of 0.0.0.0.  Yay apple.
            SetOpt("outgoing-interface:", hoststr.c_str());
            // The trampoline expects just a single source port (and sends everything back to it)
            SetOpt("outgoing-range:", "1");
            SetOpt("outgoing-port-avoid:", "0-65535");
            SetOpt("outgoing-port-permit:", "1253");
          }
#endif
        }
        // set async
        ub_ctx_async(m_ctx.get(), 1);
        // setup mainloop
#ifdef _WIN32
        running = true;
        runner = std::thread{[this]() {
          while (running)
          {
            if (m_ctx.get())
              ub_wait(m_ctx.get());
            std::this_thread::sleep_for(25ms);
          }
          if (m_ctx.get())
            ub_process(m_ctx.get());
        }};
#else
        if (auto loop = m_Loop.lock())
        {
          if (auto loop_ptr = loop->MaybeGetUVWLoop())
          {
            m_Poller = loop_ptr->resource<uvw::PollHandle>(ub_fd(m_ctx.get()));
            m_Poller->on<uvw::PollEvent>([ptr = std::weak_ptr<ub_ctx>{m_ctx}](auto&, auto&) {
              if (auto ctx = ptr.lock())
                ub_process(ctx.get());
            });
            m_Poller->start(uvw::PollHandle::Event::READABLE);
            return;
          }
        }
        throw std::runtime_error{"no uvw loop"};
#endif
      }

      void
      Down()
      {
#ifdef _WIN32
        running = false;
        runner.join();
#else
        m_Poller->close();
        if (auto loop = m_Loop.lock())
        {
          if (auto loop_ptr = loop->MaybeGetUVWLoop())
          {
            m_Poller = loop_ptr->resource<uvw::PollHandle>(ub_fd(m_ctx.get()));
            m_Poller->on<uvw::PollEvent>([ptr = std::weak_ptr<ub_ctx>{m_ctx}](auto&, auto&) {
              if (auto ctx = ptr.lock())
                ub_process(ctx.get());
            });
            m_Poller->start(uvw::PollHandle::Event::READABLE);
          }
        }
#endif
        m_ctx.reset();
      }

      int
      Rank() const override
      {
        return 10;
      }

      void
      ResetInternalState() override
      {
        Down();
        Up(m_conf);
      }

      void
      CancelPendingQueries() override
      {
        Down();
      }

      bool
      WouldLoop(const SockAddr& to, const SockAddr& from) const override
      {
#if defined(ANDROID)
        (void)to;
        (void)from;
        return false;
#else
        const auto& vec = m_conf.m_upstreamDNS;
        return std::find(vec.begin(), vec.end(), to) != std::end(vec)
            or std::find(vec.begin(), vec.end(), from) != std::end(vec);
#endif
      }

      template <typename Callable>
      void
      call(Callable&& f)
      {
        if (auto loop = m_Loop.lock())
          loop->call(std::forward<Callable>(f));
        else
          LogError("no mainloop?");
      }

      bool
      MaybeHookDNS(
          std::weak_ptr<PacketSource_Base> source,
          const Message& query,
          const SockAddr& to,
          const SockAddr& from) override
      {
        if (WouldLoop(to, from))
          return false;
        // we use this unique ptr to clean up on fail
        auto tmp = std::make_unique<Query>(weak_from_this(), query, source, to, from);
        // no questions, send fail
        if (query.questions.empty())
        {
          tmp->Cancel();
          return true;
        }

        for (const auto& q : query.questions)
        {
          // dont process .loki or .snode
          if (q.HasTLD(".loki") or q.HasTLD(".snode"))
          {
            tmp->Cancel();
            return true;
          }
        }
        // leak bare pointer and try to do the request
        auto* pending = tmp.release();
        const auto& q = query.questions[0];
        if (auto err = ub_resolve_async(
                m_ctx.get(),
                q.Name().c_str(),
                q.qtype,
                q.qclass,
                (void*)pending,
                &Resolver::Callback,
                nullptr))
        {
          // take back ownership on fail
          LogWarn("failed to send upstream query with libunbound: ", ub_strerror(err));
          tmp.reset(pending);
          tmp->Cancel();
        }
        return true;
      }
    };

    void
    Query::SendReply(llarp::OwnedBuffer replyBuf) const
    {
      auto packet_src = src.lock();
      auto parent_ptr = parent.lock();

      if (packet_src and parent_ptr)
      {
        parent_ptr->call([packet_src, from = resolverAddr, to = askerAddr, buf = replyBuf.copy()] {
          packet_src->SendTo(to, from, OwnedBuffer::copy_from(buf));
        });
      }
      else
        LogError("no source or parent");
    }
  }  // namespace libunbound

  Server::Server(EventLoop_ptr loop, llarp::DnsConfig conf)
      : m_Loop{std::move(loop)}, m_Config{std::move(conf)}
  {}

  void
  Server::Start()
  {
    // set up udp sockets
    for (const auto& addr : m_Config.m_bind)
    {
      if (auto ptr = MakePacketSourceOn(addr, m_Config))
        AddPacketSource(std::move(ptr));
    }

    // add default resolver as needed
    if (auto ptr = MakeDefaultResolver())
      AddResolver(ptr);
  }

  std::shared_ptr<PacketSource_Base>
  Server::MakePacketSourceOn(const llarp::SockAddr& addr, const llarp::DnsConfig&)
  {
    return std::make_shared<UDPReader>(*this, m_Loop, addr);
  }

  std::shared_ptr<Resolver_Base>
  Server::MakeDefaultResolver()
  {
    if (m_Config.m_upstreamDNS.empty())
    {
      LogInfo(
          "explicitly no upstream dns providers specified, we will not resolve anything but .loki "
          "and .snode");
      return nullptr;
    }

    return std::make_shared<libunbound::Resolver>(m_Loop, m_Config);
  }

  std::vector<SockAddr>
  Server::BoundPacketSourceAddrs() const
  {
    std::vector<SockAddr> addrs;
    for (const auto& src : m_PacketSources)
    {
      if (auto ptr = src.lock())
        if (auto maybe_addr = ptr->BoundOn())
          addrs.emplace_back(*maybe_addr);
    }
    return addrs;
  }

  std::optional<SockAddr>
  Server::FirstBoundPacketSourceAddr() const
  {
    for (const auto& src : m_PacketSources)
    {
      if (auto ptr = src.lock())
        if (auto bound = ptr->BoundOn())
          return bound;
    }
    return std::nullopt;
  }

  void
  Server::AddResolver(std::weak_ptr<Resolver_Base> resolver)
  {
    m_Resolvers.insert(resolver);
  }

  void
  Server::AddResolver(std::shared_ptr<Resolver_Base> resolver)
  {
    m_OwnedResolvers.insert(resolver);
    AddResolver(std::weak_ptr<Resolver_Base>{resolver});
  }

  void
  Server::AddPacketSource(std::weak_ptr<PacketSource_Base> pkt)
  {
    m_PacketSources.push_back(pkt);
  }

  void
  Server::AddPacketSource(std::shared_ptr<PacketSource_Base> pkt)
  {
    m_OwnedPacketSources.push_back(pkt);
    AddPacketSource(std::weak_ptr<PacketSource_Base>{pkt});
  }

  void
  Server::Stop()
  {
    for (const auto& resolver : m_Resolvers)
    {
      if (auto ptr = resolver.lock())
        ptr->CancelPendingQueries();
    }
  }

  void
  Server::Reset()
  {
    for (const auto& resolver : m_Resolvers)
    {
      if (auto ptr = resolver.lock())
        ptr->ResetInternalState();
    }
  }

  bool
  Server::MaybeHandlePacket(
      std::weak_ptr<PacketSource_Base> src,
      const SockAddr& to,
      const SockAddr& from,
      llarp::OwnedBuffer buf)
  {
    auto ptr = src.lock();
    if (not ptr)
      return false;
    // dont process to prevent feedback loop
    if (ptr->WouldLoop(to, from))
    {
      LogWarn("preventing dns packet replay to=", to, " from=", from);
      return false;
    }

    auto maybe = MaybeParseDNSMessage(buf);
    if (not maybe)
    {
      LogWarn("invalid dns message format from ", from, " to dns listener on ", to);
      return false;
    }
    auto& msg = *maybe;
    // we don't provide a DoH resolver because it requires verified TLS
    // TLS needs X509/ASN.1-DER and opting into the Root CA Cabal
    // thankfully mozilla added a backdoor that allows ISPs to turn it off
    // so we disable DoH for firefox using mozilla's ISP backdoor
    // see: https://github.com/oxen-io/lokinet/issues/832
    for (const auto& q : msg.questions)
    {
      // is this firefox looking for their backdoor record?
      if (q.IsName("use-application-dns.net"))
      {
        // yea it is, let's turn off DoH because god is dead.
        msg.AddNXReply();
        // press F to pay respects and send it back where it came from
        ptr->SendTo(from, to, msg.ToBuffer());
        return true;
      }
    }

    for (const auto& resolver : m_Resolvers)
    {
      if (auto res_ptr = resolver.lock())
      {
        LogDebug("check resolver ", res_ptr->ResolverName(), " for dns from ", from, " to ", to);
        if (res_ptr->MaybeHookDNS(src, msg, to, from))
          return true;
      }
    }
    return false;
  }

}  // namespace llarp::dns

#include <mempipe/mempipe.hpp>
#include <messages/discard.hpp>
#include <util/logic.hpp>
#include <util/time.hpp>
#include <ev/pipe.hpp>

namespace llarp
{
  namespace mempipe
  {
    struct MemLink;
    struct MemSession;

    struct MempipeContext
    {
      using Nodes_t =
          std::unordered_map< RouterID, LinkLayer_ptr, RouterID::Hash >;
      Nodes_t _nodes;
      using SendEvent = std::tuple< RouterID, RouterID, std::vector< byte_t >,
                                    ILinkSession::CompletionHandler >;

      /// (src, dst, session, hook)
      std::vector< SendEvent > _sendQueue;
      using NodeConnection_t = std::tuple< RouterID, RouterID >;

      struct NodeConnectionHash
      {
        size_t
        operator()(const NodeConnection_t con) const
        {
          const auto& a = std::get< 0 >(con);
          const auto& b = std::get< 1 >(con);
          auto op       = std::bit_xor< size_t >();
          return std::accumulate(a.begin(), a.end(),
                                 std::accumulate(b.begin(), b.end(), 0, op),
                                 op);
        }
      };

      using NodeConnections_t =
          std::unordered_map< NodeConnection_t, std::shared_ptr< MemSession >,
                              NodeConnectionHash >;

      NodeConnections_t _connections;

      mutable util::Mutex _access;

      void
      AddNode(LinkLayer_ptr ptr) LOCKS_EXCLUDED(_access);

      void
      RemoveNode(LinkLayer_ptr ptr) LOCKS_EXCLUDED(_access);

      LinkLayer_ptr
      FindNode(const RouterID pk) LOCKS_EXCLUDED(_access);

      /// connect src to dst
      void
      ConnectNode(const RouterID src, const RouterID dst,
                  const std::shared_ptr< MemSession >& ptr)
          LOCKS_EXCLUDED(_access);

      /// remote both src and dst as connected
      void
      DisconnectNode(const RouterID src, const RouterID dst)
          LOCKS_EXCLUDED(_access);

      bool
      HasConnection(const RouterID src, const RouterID dst) const
          LOCKS_EXCLUDED(_access);

      void
      InboundConnection(const RouterID to,
                        const std::shared_ptr< MemSession >& obsession);

      void
      CallLater(std::function< void(void) > f)
      {
        if(m_Logic && f)
          m_Logic->queue_func(f);
        else if(f)
          LogError("dropping call");
      }

      bool
      SendTo(const RouterID src, const RouterID dst,
             const std::vector< byte_t > msg,
             ILinkSession::CompletionHandler delivery) LOCKS_EXCLUDED(_access);

      void
      Pump() LOCKS_EXCLUDED(_access);

      void
      Start(llarp_ev_loop_ptr loop)
      {
        evloop = loop;
        m_Run.store(true);
        std::promise< void > p;
        m_Thread = std::make_unique< std::thread >([&]() {
          LogDebug("mempipe started");
          m_Logic = std::make_shared< Logic >();
          p.set_value();
          while(m_Run.load())
          {
            m_Logic->tick(time_now_ms());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Pump();
          }
          m_Logic->stop();
        });
        p.get_future().wait();
        LogDebug("mempipe up");
      }

      ~MempipeContext()
      {
        m_Run.store(false);
        if(m_Thread)
          m_Thread->join();
      }

      std::atomic< bool > m_Run;
      std::shared_ptr< Logic > m_Logic;
      std::unique_ptr< std::thread > m_Thread = nullptr;
      llarp_ev_loop_ptr evloop                = nullptr;
    };

    using Globals_ptr = std::unique_ptr< MempipeContext >;

    Globals_ptr _globals;

    struct MemSession : public ILinkSession,
                        public llarp_ev_pkt_pipe,
                        public std::enable_shared_from_this< MemSession >
    {
      MemSession(llarp_ev_loop_ptr ev, LinkLayer_ptr _local,
                 LinkLayer_ptr _remote, bool inbound)
          : llarp_ev_pkt_pipe(ev)
          , remote{std::move(_remote)}
          , parent{std::move(_local)}
          , isInbound{inbound}
      {
      }

      LinkLayer_ptr remote;
      LinkLayer_ptr parent;
      const bool isInbound;

      util::Mutex _access;

      std::deque< std::vector< byte_t > > m_recvQueue;
      std::deque< std::tuple< std::vector< byte_t >, CompletionHandler > >
          m_sendQueue;

      llarp_time_t lastRecv = 0;

      PubKey
      GetPubKey() const override
      {
        return remote->GetOurRC().pubkey;
      }

      bool
      SendKeepAlive() override
      {
        std::array< byte_t, 128 > pkt;
        DiscardMessage msg;
        llarp_buffer_t buf{pkt};
        if(!msg.BEncode(&buf))
          return false;
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        return SendMessageBuffer(buf, nullptr);
      }

      void
      OnRead(const llarp_buffer_t& pkt) override
      {
        std::vector< byte_t > buf;
        buf.resize(pkt.sz);
        std::copy_n(pkt.base, pkt.sz, buf.begin());
        Recv(std::move(buf));
      }

      void
      Recv(const std::vector< byte_t > msg) LOCKS_EXCLUDED(_access)
      {
        util::Lock lock(&_access);
        m_recvQueue.emplace_back(std::move(msg));
        lastRecv = parent->Now();
      }

      void
      OnLinkEstablished(ILinkLayer*) override
      {
        return;
      }

      bool
      TimedOut(llarp_time_t now) const override
      {
        return now >= lastRecv && now - lastRecv > 5000;
      }

      void
      PumpWrite() LOCKS_EXCLUDED(_access)
      {
        std::deque< std::tuple< std::vector< byte_t >, CompletionHandler > > q;
        {
          util::Lock lock(&_access);
          if(m_sendQueue.size())
            q = std::move(m_sendQueue);
        }
        const RouterID src = parent->GetOurRC().pubkey;
        const RouterID dst = GetPubKey();
        while(q.size())
        {
          const auto& f = q.front();
          _globals->SendTo(src, dst, std::get< 0 >(f), std::get< 1 >(f));
          q.pop_front();
        }
      }

      void
      PumpRead() LOCKS_EXCLUDED(_access)
      {
        std::deque< std::vector< byte_t > > q;
        {
          util::Lock lock(&_access);
          if(m_recvQueue.size())
            q = std::move(m_recvQueue);
        }
        while(q.size())
        {
          const llarp_buffer_t buf{q.front()};
          parent->HandleMessage(this, buf);
          q.pop_front();
        }
      }

      void Tick(llarp_time_t) override
      {
        Pump();
      }

      void
      Pump() override
      {
        PumpRead();
        PumpWrite();
      }

      void
      Close() override
      {
        auto self = shared_from_this();
        _globals->CallLater([=]() { self->Disconnected(); });
      }

      RouterContact
      GetRemoteRC() const override
      {
        return remote->GetOurRC();
      }

      bool
      ShouldPing() const override
      {
        return true;
      }

      bool
      SendMessageBuffer(const llarp_buffer_t& pkt,
                        ILinkSession::CompletionHandler completed) override
      {
        if(completed == nullptr)
          completed = [](ILinkSession::DeliveryStatus) {};
        auto self = shared_from_this();
        std::vector< byte_t > buf(pkt.sz);
        std::copy_n(pkt.base, pkt.sz, buf.begin());
        return _globals->SendTo(parent->GetOurRC().pubkey, GetRemoteRC().pubkey,
                                buf, [=](ILinkSession::DeliveryStatus status) {
                                  self->parent->logic()->call_later(
                                      10, std::bind(completed, status));
                                });
      }

      void
      Start() override
      {
        if(!StartPipe())
          return;
        if(isInbound)
          return;
        LogDebug("outbound start");
        auto self = shared_from_this();
        _globals->CallLater([=]() {
          LogDebug("Called inbound connection");
          _globals->InboundConnection(self->GetPubKey(), self);
        });
      }

      bool
      IsEstablished() const override
      {
        return _globals->HasConnection(parent->GetOurRC().pubkey, GetPubKey());
      }

      void
      Disconnected()
      {
        _globals->DisconnectNode(parent->GetOurRC().pubkey, GetPubKey());
      }

      bool
      RenegotiateSession() override
      {
        return true;
      }

      ILinkLayer*
      GetLinkLayer() const override
      {
        return parent.get();
      }

      util::StatusObject
      ExtractStatus() const override
      {
        return {};
      }

      llarp::Addr
      GetRemoteEndpoint() const override
      {
        return {};
      }

      size_t
      SendQueueBacklog() const override
      {
        return m_sendQueue.size();
      }
    };

    struct MemLink : public ILinkLayer,
                     public std::enable_shared_from_this< MemLink >
    {
      MemLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SignBufferFunc sign,
              SessionEstablishedHandler est, SessionRenegotiateHandler reneg,
              TimeoutHandler timeout, SessionClosedHandler closed,
              bool permitInbound)
          : ILinkLayer(routerEncSecret, getrc, h, sign, est, reneg, timeout,
                       closed)
          , allowInbound(permitInbound)
      {
      }

      const bool allowInbound;

      bool
      KeyGen(SecretKey& k) override
      {
        k.Zero();
        return true;
      }

      const char*
      Name() const override
      {
        return "mempipe";
      }

      uint16_t
      Rank() const override
      {
        return 100;
      }

      void
      Pump() override
      {
        LogDebug("memlink pump");
        std::set< RouterID > sessions;
        {
          Lock l(&m_AuthedLinksMutex);
          auto itr = m_AuthedLinks.begin();
          while(itr != m_AuthedLinks.end())
          {
            sessions.insert(itr->first);
            ++itr;
          }
        }
        ILinkLayer::Pump();
        {
          Lock l(&m_AuthedLinksMutex);
          for(const auto& pk : sessions)
          {
            if(m_AuthedLinks.count(pk) == 0)
            {
              // all sessions were removed
              SessionClosed(pk);
            }
          }
        }
      }

      void
      RecvFrom(const llarp::Addr&, const void*, size_t) override
      {
      }

      bool
      Configure(llarp_ev_loop_ptr ev, const std::string&, int,
                uint16_t) override
      {
        m_Loop = ev;
        if(_globals == nullptr)
        {
          _globals = std::make_unique< MempipeContext >();
          _globals->Start(ev);
        }
        return _globals != nullptr;
      }

      std::shared_ptr< ILinkSession >
      NewOutboundSession(const RouterContact& rc,
                         const AddressInfo& ai) override
      {
        if(ai.dialect != Name())
          return nullptr;
        auto remote = _globals->FindNode(rc.pubkey);
        if(remote == nullptr)
          return nullptr;
        return std::make_shared< MemSession >(m_Loop, shared_from_this(),
                                              remote, false);
      }

      bool
      Start(std::shared_ptr< Logic > l) override
      {
        if(!ILinkLayer::Start(l))
          return false;
        _globals->AddNode(shared_from_this());
        return true;
      }

      void
      Stop() override
      {
        _globals->RemoveNode(shared_from_this());
      }
    };

    LinkLayer_ptr
    NewOutboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                    LinkMessageHandler h, SignBufferFunc sign,
                    SessionEstablishedHandler est,
                    SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                    SessionClosedHandler closed)
    {
      return std::make_shared< MemLink >(routerEncSecret, getrc, h, sign, est,
                                         reneg, timeout, closed, false);
    }

    LinkLayer_ptr
    NewInboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                   LinkMessageHandler h, SignBufferFunc sign,
                   SessionEstablishedHandler est,
                   SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                   SessionClosedHandler closed)
    {
      return std::make_shared< MemLink >(routerEncSecret, getrc, h, sign, est,
                                         reneg, timeout, closed, true);
    }

    void
    MempipeContext::AddNode(LinkLayer_ptr ptr)
    {
      util::Lock lock(&_access);
      _nodes.emplace(RouterID(ptr->GetOurRC().pubkey), ptr);
      LogInfo("add mempipe node: ", RouterID(ptr->GetOurRC().pubkey));
    }

    bool
    MempipeContext::SendTo(const RouterID src, const RouterID dst,
                           const std::vector< byte_t > msg,
                           ILinkSession::CompletionHandler delivery)
    {
      util::Lock lock(&_access);
      _sendQueue.emplace_back(std::move(src), std::move(dst), std::move(msg),
                              std::move(delivery));
      return true;
    }

    void
    MempipeContext::InboundConnection(const RouterID to,
                                      const std::shared_ptr< MemSession >& ob)
    {
      LogDebug("inbound connect to ", to, " from ",
               RouterID(ob->parent->GetOurRC().pubkey));
      std::shared_ptr< MemSession > other;
      {
        util::Lock lock(&_access);
        auto itr = _nodes.find(to);
        if(itr != _nodes.end())
        {
          other = std::make_shared< MemSession >(evloop, itr->second,
                                                 ob->parent, true);
        }
      }
      if(other)
      {
        ConnectNode(other->GetPubKey(), ob->GetPubKey(), other);
        ConnectNode(ob->GetPubKey(), other->GetPubKey(), ob);
        ob->parent->logic()->queue_func([ob]() {
          ob->parent->MapAddr(RouterID{ob->GetPubKey()}, ob.get());
          ob->parent->SessionEstablished(ob.get());
        });
        other->parent->logic()->queue_func([other]() {
          other->parent->MapAddr(RouterID{other->GetPubKey()}, other.get());
          other->parent->SessionEstablished(other.get());
        });
      }
      else
      {
        ob->Disconnected();
      }
    }

    void
    MempipeContext::ConnectNode(const RouterID src, const RouterID dst,
                                const std::shared_ptr< MemSession >& session)
    {
      LogDebug("connect ", src, " to ", dst);
      util::Lock lock(&_access);
      _connections.emplace(std::make_pair(std::make_tuple(src, dst), session));
    }

    void
    MempipeContext::DisconnectNode(const RouterID src, const RouterID dst)
    {
      LogDebug("connect ", src, " from ", dst);
      util::Lock lock(&_access);
      _connections.erase({src, dst});
    }

    LinkLayer_ptr
    MempipeContext::FindNode(const RouterID rid)
    {
      util::Lock lock(&_access);
      auto itr = _nodes.find(rid);
      if(itr == _nodes.end())
        return nullptr;
      return itr->second;
    }

    bool
    MempipeContext::HasConnection(const RouterID src, const RouterID dst) const
    {
      util::Lock lock(&_access);
      return _connections.find({src, dst}) != _connections.end();
    }

    void
    MempipeContext::RemoveNode(LinkLayer_ptr node)
    {
      util::Lock lock(&_access);
      const RouterID pk = node->GetOurRC().pubkey;
      _nodes.erase(pk);
      auto itr = _connections.begin();
      while(itr != _connections.end())
      {
        if(std::get< 0 >(itr->first) == pk || std::get< 1 >(itr->first) == pk)
        {
          auto s = itr->second->shared_from_this();
          itr->second->GetLinkLayer()->logic()->call_later(
              1, [s]() { s->Disconnected(); });
        }
        ++itr;
      }
    }

    void
    MempipeContext::Pump()
    {
      std::vector< SendEvent > q;
      {
        util::Lock lock(&_access);
        q = std::move(_sendQueue);
      }
      for(auto& f : q)
      {
        ILinkSession::DeliveryStatus status =
            ILinkSession::DeliveryStatus::eDeliveryDropped;
        {
          util::Lock lock(&_access);
          auto itr = _connections.find({std::get< 0 >(f), std::get< 1 >(f)});
          if(itr != _connections.end())
          {
            const llarp_buffer_t pkt{std::get< 2 >(f)};
            if(itr->second->Write(pkt))
              status = ILinkSession::DeliveryStatus::eDeliverySuccess;
          }
        }
        LogDebug(std::get< 0 >(f), "->", std::get< 1 >(f),
                 " status=", (int)status);
        CallLater(std::bind(std::get< 3 >(f), status));
      }
    }
  }  // namespace mempipe
}  // namespace llarp
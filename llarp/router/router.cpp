#include <router/router.hpp>

#include <config.hpp>
#include <constants/proto.hpp>
#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <dht/context.hpp>
#include <dht/node.hpp>
#include <iwp/iwp.hpp>
#include <link/server.hpp>
#include <messages/link_message.hpp>
#include <net/net.hpp>
#include <rpc/rpc.hpp>
#include <util/buffer.hpp>
#include <util/encode.hpp>
#include <util/logger.hpp>
#include <util/memfn.hpp>
#include <util/file_logger.hpp>
#include <util/logger_syslog.hpp>
#include <util/metrics.hpp>
#include <util/str.hpp>
#include <utp/utp.hpp>

#include <fstream>
#include <cstdlib>
#include <iterator>
#if defined(RPI) || defined(ANDROID)
#include <unistd.h>
#endif

namespace llarp
{
  struct async_verify_context
  {
    Router *router;
    TryConnectJob *establish_job;
  };

}  // namespace llarp

struct TryConnectJob
{
  llarp_time_t lastAttempt = 0;
  const llarp::RouterContact rc;
  llarp::LinkLayer_ptr link;
  llarp::Router *router;
  uint16_t triesLeft;
  TryConnectJob(const llarp::RouterContact &remote, llarp::LinkLayer_ptr l,
                uint16_t tries, llarp::Router *r)
      : rc(remote), link(l), router(r), triesLeft(tries)
  {
  }

  ~TryConnectJob()
  {
  }

  bool
  TimeoutReached() const
  {
    const auto now = router->Now();
    return now > lastAttempt && now - lastAttempt > 5000;
  }

  void
  Success()
  {
    router->routerProfiling().MarkConnectSuccess(rc.pubkey);
    router->FlushOutboundFor(rc.pubkey, link.get());
  }

  /// return true to remove
  bool
  Timeout()
  {
    if(ShouldRetry())
    {
      return Attempt();
    }
    // discard pending traffic on timeout
    router->DiscardOutboundFor(rc.pubkey);
    router->routerProfiling().MarkConnectTimeout(rc.pubkey);
    if(router->routerProfiling().IsBad(rc.pubkey))
    {
      if(!router->IsBootstrapNode(rc.pubkey))
        router->nodedb()->Remove(rc.pubkey);
    }
    return true;
  }

  /// return true to remove
  bool
  Attempt()
  {
    --triesLeft;
    if(!link)
      return true;
    if(!link->TryEstablishTo(rc))
      return true;
    lastAttempt = router->Now();
    return false;
  }

  bool
  ShouldRetry() const
  {
    return triesLeft > 0;
  }
};

static void
on_try_connecting(std::shared_ptr< TryConnectJob > j)
{
  if(j->Attempt())
    j->router->pendingEstablishJobs.erase(j->rc.pubkey);
}

bool
llarp_loadServiceNodeIdentityKey(const fs::path &fpath,
                                 llarp::SecretKey &secret)
{
  std::string path = fpath.string();
  llarp::IdentitySecret ident;

  if(!ident.LoadFromFile(path.c_str()))
    return false;

  return llarp::CryptoManager::instance()->seed_to_secretkey(secret, ident);
}

bool
llarp_findOrCreateIdentity(const fs::path &path, llarp::SecretKey &secretkey)
{
  std::string fpath = path.string();
  llarp::LogDebug("find or create ", fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::LogInfo("generating new identity key");
    llarp::CryptoManager::instance()->identity_keygen(secretkey);
    if(!secretkey.SaveToFile(fpath.c_str()))
      return false;
  }
  return secretkey.LoadFromFile(fpath.c_str());
}

// C++ ...
bool
llarp_findOrCreateEncryption(const fs::path &path, llarp::SecretKey &encryption)
{
  std::string fpath = path.string();
  llarp::LogDebug("find or create ", fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::LogInfo("generating new encryption key");
    llarp::CryptoManager::instance()->encryption_keygen(encryption);
    if(!encryption.SaveToFile(fpath.c_str()))
      return false;
  }
  return encryption.LoadFromFile(fpath.c_str());
}

namespace llarp
{
  bool
  Router::TryConnectAsync(RouterContact remote, uint16_t numretries)
  {
    const RouterID us = pubkey();
    if(remote.pubkey == us)
      return false;
    if(!ConnectionToRouterAllowed(remote.pubkey))
      return false;
    // do we already have a pending job for this remote?
    if(HasPendingConnectJob(remote.pubkey))
    {
      LogDebug("We have pending connect jobs to ", remote.pubkey);
      return false;
    }

    for(auto &link : outboundLinks)
    {
      if(!link->IsCompatable(remote))
        continue;
      std::shared_ptr< TryConnectJob > job =
          std::make_shared< TryConnectJob >(remote, link, numretries, this);
      auto itr = pendingEstablishJobs.emplace(remote.pubkey, job);
      if(itr.second)
      {
        // try establishing async
        _logic->queue_func(std::bind(&on_try_connecting, job));
        return true;
      }
      else
      {
        itr.first->second->Attempt();
      }
    }
    return false;
  }

  bool
  Router::OnSessionEstablished(ILinkSession *s)
  {
    return async_verify_RC(s->GetRemoteRC());
  }

  Router::Router(struct llarp_threadpool *_tp, llarp_ev_loop_ptr __netloop,
                 std::shared_ptr< Logic > l)
      : ready(false)
      , _netloop(__netloop)
      , tp(_tp)
      , _logic(l)
      , paths(this)
      , _exitContext(this)
      , disk(1, 1000)
      , _dht(llarp_dht_context_new(this))
      , inbound_link_msg_parser(this)
      , _hiddenServiceContext(this)
  {
    // set rational defaults
    this->ip4addr.sin_family = AF_INET;
    this->ip4addr.sin_port   = htons(1090);

    _stopping.store(false);
    _running.store(false);
  }

  Router::~Router()
  {
    llarp_dht_context_free(_dht);
  }

  util::StatusObject
  Router::ExtractStatus() const
  {
    util::StatusObject obj{{"dht", _dht->impl->ExtractStatus()},
                           {"services", _hiddenServiceContext.ExtractStatus()},
                           {"exit", _exitContext.ExtractStatus()}};
    std::vector< util::StatusObject > ob_links, ib_links;
    std::transform(inboundLinks.begin(), inboundLinks.end(),
                   std::back_inserter(ib_links),
                   [](const auto &link) -> util::StatusObject {
                     return link->ExtractStatus();
                   });
    std::transform(outboundLinks.begin(), outboundLinks.end(),
                   std::back_inserter(ob_links),
                   [](const auto &link) -> util::StatusObject {
                     return link->ExtractStatus();
                   });
    obj.Put("links",
            util::StatusObject{{"outbound", ob_links}, {"inbound", ib_links}});
    return obj;
  }

  bool
  Router::HandleRecvLinkMessageBuffer(ILinkSession *session,
                                      const llarp_buffer_t &buf)
  {
    if(_stopping)
      return true;

    if(!session)
    {
      LogWarn("no link session");
      return false;
    }
    return inbound_link_msg_parser.ProcessFrom(session, buf);
  }

  void
  Router::PersistSessionUntil(const RouterID &remote, llarp_time_t until)
  {
    m_PersistingSessions[remote] =
        std::max(until, m_PersistingSessions[remote]);
    LogDebug("persist session to ", remote, " until ",
             m_PersistingSessions[remote]);
  }

  bool
  Router::GetRandomGoodRouter(RouterID &router)
  {
    auto pick_router = [&](auto &collection) -> bool {
      const auto sz = collection.size();
      auto itr      = collection.begin();
      if(sz == 0)
        return false;
      if(sz > 1)
        std::advance(itr, randint() % sz);
      router = itr->first;
      return true;
    };
    if(whitelistRouters)
    {
      pick_router(lokinetRouters);
    }
    absl::ReaderMutexLock l(&nodedb()->access);
    return pick_router(nodedb()->entries);
  }

  void
  Router::PumpLL()
  {
    for(const auto &link : inboundLinks)
    {
      link->Pump();
    }
    for(const auto &link : outboundLinks)
    {
      link->Pump();
    }
  }

  bool
  Router::SendToOrQueue(const RouterID &remote, const ILinkMessage *msg)
  {
    for(const auto &link : inboundLinks)
    {
      if(link->HasSessionTo(remote))
      {
        SendTo(remote, msg, link.get());
        return true;
      }
    }
    for(const auto &link : outboundLinks)
    {
      if(link->HasSessionTo(remote))
      {
        SendTo(remote, msg, link.get());
        return true;
      }
    }
    // no link available

    // this will create an entry in the outbound mq if it's not already there
    auto itr = outboundMessageQueue.find(remote);
    if(itr == outboundMessageQueue.end())
    {
      outboundMessageQueue.emplace(remote, MessageQueue());
    }
    // encode
    llarp_buffer_t buf(linkmsg_buffer);
    if(!msg->BEncode(&buf))
      return false;
    // queue buffer
    auto &q = outboundMessageQueue[remote];

    buf.sz = buf.cur - buf.base;
    q.emplace(buf.sz);
    memcpy(q.back().data(), buf.base, buf.sz);
    RouterContact remoteRC;
    // we don't have an open session to that router right now
    if(nodedb()->Get(remote, remoteRC))
    {
      // try connecting directly as the rc is loaded from disk
      return TryConnectAsync(remoteRC, 10);
    }

    // we don't have the RC locally so do a dht lookup
    _dht->impl->LookupRouter(remote,
                             std::bind(&Router::HandleDHTLookupForSendTo, this,
                                       remote, std::placeholders::_1));
    return true;
  }

  void
  Router::HandleDHTLookupForSendTo(RouterID remote,
                                   const std::vector< RouterContact > &results)
  {
    if(results.size())
    {
      if(whitelistRouters
         && lokinetRouters.find(results[0].pubkey) == lokinetRouters.end())
      {
        return;
      }
      if(results[0].Verify(Now()))
      {
        TryConnectAsync(results[0], 10);
        return;
      }
    }
    DiscardOutboundFor(remote);
  }

  void
  Router::ForEachPeer(std::function< void(const ILinkSession *, bool) > visit,
                      bool randomize) const
  {
    for(const auto &link : outboundLinks)
    {
      link->ForEachSession(
          [visit](const ILinkSession *peer) { visit(peer, true); }, randomize);
    }
    for(const auto &link : inboundLinks)
    {
      link->ForEachSession(
          [visit](const ILinkSession *peer) { visit(peer, false); }, randomize);
    }
  }

  void
  Router::ForEachPeer(std::function< void(ILinkSession *) > visit)
  {
    for(const auto &link : outboundLinks)
    {
      link->ForEachSession([visit](ILinkSession *peer) { visit(peer); });
    }
    for(const auto &link : inboundLinks)
    {
      link->ForEachSession([visit](ILinkSession *peer) { visit(peer); });
    }
  }

  void
  Router::try_connect(fs::path rcfile)
  {
    RouterContact remote;
    if(!remote.Read(rcfile.string().c_str()))
    {
      LogError("failure to decode or verify of remote RC");
      return;
    }
    if(remote.Verify(Now()))
    {
      LogDebug("verified signature");
      if(!TryConnectAsync(remote, 10))
      {
        // or error?
        LogWarn("session already made");
      }
    }
    else
      LogError(rcfile, " contains invalid RC");
  }

  bool
  Router::EnsureIdentity()
  {
    if(!EnsureEncryptionKey())
      return false;
    if(usingSNSeed)
      return llarp_loadServiceNodeIdentityKey(ident_keyfile, _identity);
    else
      return llarp_findOrCreateIdentity(ident_keyfile, _identity);
  }

  bool
  Router::EnsureEncryptionKey()
  {
    return llarp_findOrCreateEncryption(encryption_keyfile, _encryption);
  }

  void
  Router::AddLink(std::shared_ptr< ILinkLayer > link, bool inbound)
  {
    if(inbound)
      inboundLinks.emplace(link);
    else
      outboundLinks.emplace(link);
  }

  bool
  Router::Configure(Config *conf)
  {
    conf->visit(util::memFn(&Router::router_iter_config, this));
    if(!InitOutboundLinks())
      return false;
    if(!Ready())
    {
      return false;
    }
    return EnsureIdentity();
  }

  bool
  Router::Ready()
  {
    return outboundLinks.size() > 0;
  }

  /// called in disk worker thread
  void
  Router::HandleSaveRC() const
  {
    std::string fname = our_rc_file.string();
    _rc.Write(fname.c_str());
  }

  bool
  Router::SaveRC()
  {
    LogDebug("verify RC signature");
    if(!_rc.Verify(Now()))
    {
      Dump< MAX_RC_SIZE >(rc());
      LogError("RC is invalid, not saving");
      return false;
    }
    diskworker()->addJob(std::bind(&Router::HandleSaveRC, this));
    return true;
  }

  bool
  Router::IsServiceNode() const
  {
    return inboundLinks.size() > 0;
  }

  void
  Router::Close()
  {
    LogInfo("closing router");
    llarp_ev_loop_stop(_netloop.get());
    inboundLinks.clear();
    outboundLinks.clear();
    disk.stop();
    disk.shutdown();
  }

  void
  Router::on_verify_client_rc(llarp_async_verify_rc *job)
  {
    async_verify_context *ctx =
        static_cast< async_verify_context * >(job->user);
    auto router = ctx->router;
    const PubKey pk(job->rc.pubkey);
    router->FlushOutboundFor(pk, router->GetLinkWithSessionByPubkey(pk));
    delete ctx;
    router->pendingVerifyRC.erase(pk);
    router->pendingEstablishJobs.erase(pk);
  }

  void
  Router::on_verify_server_rc(llarp_async_verify_rc *job)
  {
    async_verify_context *ctx =
        static_cast< async_verify_context * >(job->user);
    auto router = ctx->router;
    const PubKey pk(job->rc.pubkey);
    if(!job->valid)
    {
      delete ctx;
      router->DiscardOutboundFor(pk);
      router->pendingVerifyRC.erase(pk);
      return;
    }
    // we're valid, which means it's already been committed to the nodedb

    LogDebug("rc verified and saved to nodedb");

    if(router->validRouters.count(pk))
    {
      router->validRouters.erase(pk);
    }

    const RouterContact rc = job->rc;

    router->validRouters.emplace(pk, rc);

    // track valid router in dht
    router->dht()->impl->Nodes()->PutNode(rc);

    // mark success in profile
    router->routerProfiling().MarkConnectSuccess(pk);

    // this was an outbound establish job
    if(ctx->establish_job)
    {
      ctx->establish_job->Success();
    }
    else
      router->FlushOutboundFor(pk, router->GetLinkWithSessionByPubkey(pk));
    delete ctx;
    router->pendingVerifyRC.erase(pk);
  }

  void
  Router::handle_router_ticker(void *user, uint64_t orig, uint64_t left)
  {
    if(left)
      return;
    Router *self        = static_cast< Router * >(user);
    self->ticker_job_id = 0;
    self->Tick();
    self->ScheduleTicker(orig);
  }

  bool
  Router::ParseRoutingMessageBuffer(const llarp_buffer_t &buf,
                                    routing::IMessageHandler *h,
                                    const PathID_t &rxid)
  {
    return inbound_routing_msg_parser.ParseMessageBuffer(buf, h, rxid, this);
  }

  bool
  Router::ConnectionToRouterAllowed(const RouterID &router) const
  {
    if(strictConnectPubkeys.size() && strictConnectPubkeys.count(router) == 0)
      return false;
    else if(IsServiceNode() && whitelistRouters)
      return lokinetRouters.find(router) != lokinetRouters.end();
    else
      return true;
  }

  void
  Router::HandleDHTLookupForExplore(RouterID,
                                    const std::vector< RouterContact > &results)
  {
    const auto numConnected = NumberOfConnectedRouters();
    for(const auto &rc : results)
    {
      if(!rc.Verify(Now()))
        continue;
      nodedb()->InsertAsync(rc);

      if(ConnectionToRouterAllowed(rc.pubkey)
         && numConnected < minConnectedRouters)
        TryConnectAsync(rc, 10);
    }
  }

  void
  Router::TryEstablishTo(const RouterID &remote)
  {
    const RouterID us = pubkey();
    if(us == remote)
      return;

    if(!ConnectionToRouterAllowed(remote))
    {
      LogWarn("not connecting to ", remote, " as it's not permitted by config");
      return;
    }

    RouterContact rc;
    if(nodedb()->Get(remote, rc))
    {
      // try connecting async
      TryConnectAsync(rc, 5);
    }
    else if(IsServiceNode())
    {
      if(dht()->impl->HasRouterLookup(remote))
        return;
      LogInfo("looking up router ", remote);
      // dht lookup as we don't know it
      dht()->impl->LookupRouter(
          remote,
          std::bind(&Router::HandleDHTLookupForTryEstablishTo, this, remote,
                    std::placeholders::_1));
    }
    else
    {
      LogWarn("not connecting to ", remote, " as it's unreliable");
    }
  }

  void
  Router::OnConnectTimeout(ILinkSession *session)
  {
    auto itr = pendingEstablishJobs.find(session->GetPubKey());
    if(itr != pendingEstablishJobs.end())
    {
      if(itr->second->Timeout())
        pendingEstablishJobs.erase(itr);
    }
  }

  void
  Router::HandleDHTLookupForTryEstablishTo(
      RouterID remote, const std::vector< RouterContact > &results)
  {
    if(results.size() == 0)
    {
      if(!IsServiceNode())
        routerProfiling().MarkConnectTimeout(remote);
    }
    for(const auto &result : results)
    {
      if(whitelistRouters
         && lokinetRouters.find(result.pubkey) == lokinetRouters.end())
        continue;
      TryConnectAsync(result, 10);
    }
  }

  size_t
  Router::NumberOfRoutersMatchingFilter(
      std::function< bool(const ILinkSession *) > filter) const
  {
    std::set< RouterID > connected;
    ForEachPeer([&](const auto *link, bool) {
      if(filter(link))
        connected.insert(link->GetPubKey());
    });
    return connected.size();
  }

  size_t
  Router::NumberOfConnectedRouters() const
  {
    return NumberOfRoutersMatchingFilter([&](const ILinkSession *link) -> bool {
      if(!link->IsEstablished())
        return false;
      const RouterContact rc(link->GetRemoteRC());
      return rc.IsPublicRouter() && ConnectionToRouterAllowed(rc.pubkey);
    });
  }

  size_t
  Router::NumberOfConnectedClients() const
  {
    return NumberOfRoutersMatchingFilter([&](const ILinkSession *link) -> bool {
      if(!link->IsEstablished())
        return false;
      const RouterContact rc(link->GetRemoteRC());
      return !rc.IsPublicRouter();
    });
  }

  size_t
  Router::NumberOfConnectionsMatchingFilter(
      std::function< bool(const ILinkSession *) > filter) const
  {
    size_t sz = 0;
    ForEachPeer([&](const auto *link, bool) {
      if(filter(link))
        ++sz;
    });
    return sz;
  }

  bool
  Router::UpdateOurRC(bool rotateKeys)
  {
    SecretKey nextOnionKey;
    RouterContact nextRC = _rc;
    if(rotateKeys)
    {
      CryptoManager::instance()->encryption_keygen(nextOnionKey);
      std::string f = encryption_keyfile.string();
      // TODO: use disk worker
      if(nextOnionKey.SaveToFile(f.c_str()))
      {
        nextRC.enckey = seckey_topublic(nextOnionKey);
        _encryption   = nextOnionKey;
      }
    }
    nextRC.last_updated = Now();
    if(!nextRC.Sign(identity()))
      return false;
    _rc = nextRC;
    // propagate RC by renegotiating sessions
    ForEachPeer([](ILinkSession *s) {
      if(s->RenegotiateSession())
        LogInfo("renegotiated session");
      else
        LogWarn("failed to renegotiate session");
    });

    return SaveRC();
  }

  void
  Router::router_iter_config(const char *section, const char *key,
                             const char *val)
  {
    llarp::LogDebug(section, " ", key, "=", val);

    int af;
    uint16_t proto = 0;
    std::set< std::string > opts;
    if(StrEq(val, "eth"))
    {
#ifdef AF_LINK
      af = AF_LINK;
#endif
#ifdef AF_PACKET
      af = AF_PACKET;
#endif
      proto = LLARP_ETH_PROTO;
    }
    else if(StrEq(section, "bind"))
    {
      // try IPv4 first
      af = AF_INET;
      std::set< std::string > parsed_opts;
      std::string v = val;
      std::string::size_type idx;
      do
      {
        idx = v.find_first_of(',');
        if(idx != std::string::npos)
        {
          parsed_opts.insert(v.substr(0, idx));
          v = v.substr(idx + 1);
        }
        else
          parsed_opts.insert(v);
      } while(idx != std::string::npos);

      /// for each option
      for(const auto &item : parsed_opts)
      {
        /// see if it's a number
        auto port = std::atoi(item.c_str());
        if(port > 0)
        {
          /// set port
          if(proto == 0)
            proto = port;
        }  /// otherwise add to opts
        else
          opts.insert(item);
      }
    }

    if(StrEq(section, "bind"))
    {
      if(StrEq(key, "*"))
      {
        m_OutboundPort = proto;
      }
      else
      {
        auto server = llarp::utp::NewServerFromRouter(this);
        if(!server->EnsureKeys(transport_keyfile.string().c_str()))
        {
          llarp::LogError("failed to ensure keyfile ", transport_keyfile);
          return;
        }
        if(server->Configure(netloop(), key, af, proto))
        {
          AddLink(std::move(server), true);
          return;
        }
        LogError("failed to bind inbound link on ", key, " port ", proto);
      }
    }
    else if(StrEq(section, "network"))
    {
      if(StrEq(key, "profiling"))
      {
        if(IsTrueValue(val))
        {
          routerProfiling().Enable();
          LogInfo("router profiling explicitly enabled");
        }
        else if(IsFalseValue(val))
        {
          routerProfiling().Disable();
          LogInfo("router profiling explicitly disabled");
        }
      }
      if(StrEq(key, "profiles"))
      {
        routerProfilesFile = val;
        routerProfiling().Load(val);
        llarp::LogInfo("setting profiles to ", routerProfilesFile);
      }
      else if(StrEq(key, "strict-connect"))
      {
        if(IsServiceNode())
        {
          llarp::LogError("cannot use strict-connect option as service node");
          return;
        }
        llarp::RouterID snode;
        llarp::PubKey pk;
        if(pk.FromString(val))
        {
          if(strictConnectPubkeys.emplace(pk).second)
            llarp::LogInfo("added ", pk, " to strict connect list");
          else
            llarp::LogWarn("duplicate key for strict connect: ", pk);
        }
        else if(snode.FromString(val))
        {
          if(strictConnectPubkeys.insert(snode).second)
          {
            llarp::LogInfo("added ", snode, " to strict connect list");
            netConfig.emplace(key, val);
          }
          else
            llarp::LogWarn("duplicate key for strict connect: ", snode);
        }
        else
          llarp::LogError("invalid key for strict-connect: ", val);
      }
      else
      {
        netConfig.emplace(key, val);
      }
    }
    else if(StrEq(section, "api"))
    {
      if(StrEq(key, "enabled"))
      {
        enableRPCServer = IsTrueValue(val);
      }
      if(StrEq(key, "bind"))
      {
        rpcBindAddr = val;
      }
      if(StrEq(key, "authkey"))
      {
        // TODO: add pubkey to whitelist
      }
    }
    else if(StrEq(section, "services"))
    {
      if(LoadHiddenServiceConfig(val))
      {
        llarp::LogInfo("loaded hidden service config for ", key);
      }
      else
      {
        llarp::LogWarn("failed to load hidden service config for ", key);
      }
    }
    else if(StrEq(section, "logging"))
    {
      if(StrEq(key, "type") && StrEq(val, "syslog"))
      {
        // TODO(despair): write event log syslog class
#if defined(_WIN32)
        LogError("syslog not supported on win32");
#else
        LogInfo("Switching to syslog");
        LogContext::Instance().logStream = std::make_unique< SysLogStream >();
#endif
      }
      if(StrEq(key, "file"))
      {
        LogInfo("open log file: ", val);
        FILE *const logfile = ::fopen(val, "a");
        if(logfile)
        {
          LogContext::Instance().logStream =
              std::make_unique< FileLogStream >(diskworker(), logfile, 500);
          LogInfo("started logging to ", val);
        }
        else if(errno)
        {
          LogError("could not open log file at '", val, "': ", strerror(errno));
          errno = 0;
        }
        else
        {
          LogError("failed to open log file at '", val,
                   "' for an unknown reason, bailing tf out kbai");
          ::abort();
        }
      }
    }
    else if(StrEq(section, "lokid"))
    {
      if(StrEq(key, "service-node-seed"))
      {
        usingSNSeed   = true;
        ident_keyfile = val;
      }
      if(StrEq(key, "enabled"))
      {
        whitelistRouters = IsTrueValue(val);
      }
      if(StrEq(key, "jsonrpc") || StrEq(key, "addr"))
      {
        lokidRPCAddr = val;
      }
      if(StrEq(key, "username"))
      {
        lokidRPCUser = val;
      }
      if(StrEq(key, "password"))
      {
        lokidRPCPassword = val;
      }
    }
    else if(StrEq(section, "dns"))
    {
      if(StrEq(key, "upstream"))
      {
        llarp::LogInfo("add upstream resolver ", val);
        netConfig.emplace("upstream-dns", val);
      }
      if(StrEq(key, "bind"))
      {
        llarp::LogInfo("set local dns to ", val);
        netConfig.emplace("local-dns", val);
      }
    }
    else if(StrEq(section, "connect")
            || (StrEq(section, "bootstrap") && StrEq(key, "add-node")))
    {
      // llarp::LogDebug("connect section has ", key, "=", val);
      RouterContact rc;
      if(!rc.Read(val))
      {
        llarp::LogWarn("failed to decode bootstrap RC, file='", val,
                       "' rc=", rc);
        ;
        return;
      }
      if(rc.Verify(Now()))
      {
        const auto result = bootstrapRCList.insert(rc);
        if(result.second)
          llarp::LogInfo("Added bootstrap node ", RouterID(rc.pubkey));
        else
          llarp::LogWarn("Duplicate bootstrap node ", RouterID(rc.pubkey));
      }
      else
      {
        if(rc.IsExpired(Now()))
        {
          llarp::LogWarn("Bootstrap node ", RouterID(rc.pubkey),
                         " is too old and needs to be refreshed");
        }
        else
        {
          llarp::LogError("malformed rc file='", val, "' rc=", rc);
        }
      }
    }
    else if(StrEq(section, "router"))
    {
      if(StrEq(key, "netid"))
      {
        if(strlen(val) <= _rc.netID.size())
        {
          llarp::LogWarn("!!!! you have manually set netid to be '", val,
                         "' which does not equal '", Version::LLARP_NET_ID,
                         "' you will run as a different network, good luck "
                         "and "
                         "don't forget: something something MUH traffic "
                         "shape "
                         "correlation !!!!");
          llarp::NetID::DefaultValue() =
              llarp::NetID(reinterpret_cast< const byte_t * >(strdup(val)));
          // re set netid in our rc
          _rc.netID = llarp::NetID();
        }
        else
          llarp::LogError("invalid netid '", val, "', is too long");
      }
      if(StrEq(key, "max-connections"))
      {
        auto ival = atoi(val);
        if(ival > 0)
        {
          maxConnectedRouters = ival;
          LogInfo("max connections set to ", maxConnectedRouters);
        }
      }
      if(StrEq(key, "min-connections"))
      {
        auto ival = atoi(val);
        if(ival > 0)
        {
          minConnectedRouters = ival;
          LogInfo("min connections set to ", minConnectedRouters);
        }
      }
      if(StrEq(key, "nickname"))
      {
        _rc.SetNick(val);
        // set logger name here
        LogContext::Instance().nodeName = rc().Nick();
      }
      if(StrEq(key, "encryption-privkey"))
      {
        encryption_keyfile = val;
      }
      if(StrEq(key, "contact-file"))
      {
        our_rc_file = val;
      }
      if(StrEq(key, "transport-privkey"))
      {
        transport_keyfile = val;
      }
      if((StrEq(key, "identity-privkey") || StrEq(key, "ident-privkey"))
         && !usingSNSeed)
      {
        ident_keyfile = val;
      }
      if(StrEq(key, "public-address") || StrEq(key, "public-ip"))
      {
        llarp::LogInfo("public ip ", val, " size ", strlen(val));
        if(strlen(val) < 17)
        {
          // assume IPv4
          // inet_pton(AF_INET, val, &ip4addr.sin_addr);
          // struct sockaddr dest;
          // sockaddr *dest = (sockaddr *)&ip4addr;
          llarp::Addr a(val);
          llarp::LogInfo("setting public ipv4 ", a);
          addrInfo.ip    = *a.addr6();
          publicOverride = true;
        }
        // llarp::Addr a(val);
      }
      if(StrEq(key, "public-port"))
      {
        llarp::LogInfo("Setting public port ", val);
        int p = atoi(val);
        // Not needed to flip upside-down - this is done in llarp::Addr(const
        // AddressInfo&)
        ip4addr.sin_port = p;
        addrInfo.port    = p;
        publicOverride   = true;
      }
    }
  }

  bool
  Router::CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc)
  {
    // missmatch of identity ?
    if(newrc.pubkey != oldrc.pubkey)
      return false;

    // store it in nodedb async
    if(!async_verify_RC(newrc))
      return false;
    // update dht if required
    if(dht()->impl->Nodes()->HasNode(dht::Key_t{newrc.pubkey}))
    {
      dht()->impl->Nodes()->PutNode(newrc);
    }
    // update valid routers
    {
      auto itr = validRouters.find(newrc.pubkey);
      if(itr == validRouters.end())
        validRouters[newrc.pubkey] = newrc;
      else
        itr->second = newrc;
    }
    // TODO: check for other places that need updating the RC
    return true;
  }

  void
  Router::ServiceNodeLookupRouterWhenExpired(RouterID router)
  {
    using namespace std::placeholders;
    dht()->impl->LookupRouter(
        router,
        std::bind(&Router::HandleDHTLookupForExplore, this, router, _1));
  }

  void
  Router::LookupRouter(RouterID remote, RouterLookupHandler resultHandler)
  {
    if(IsServiceNode())
    {
      if(resultHandler)
        dht()->impl->LookupRouter(remote, resultHandler);
      else
        ServiceNodeLookupRouterWhenExpired(remote);
      return;
    }
    _hiddenServiceContext.ForEachService(
        [=](const std::string &,
            const std::shared_ptr< service::Endpoint > &ep) -> bool {
          return !ep->LookupRouterAnon(remote, resultHandler);
        });
  }

  bool
  Router::IsBootstrapNode(const RouterID r) const
  {
    return std::count_if(
               bootstrapRCList.begin(), bootstrapRCList.end(),
               [r](const RouterContact &rc) -> bool { return rc.pubkey == r; })
        > 0;
  }

  void
  Router::Tick()
  {
    if(_stopping)
      return;
    // LogDebug("tick router");
    auto now = Now();

    routerProfiling().Tick();

    if(IsServiceNode())
    {
      if(_rc.ExpiresSoon(now, randint() % 10000)
         || (now - _rc.last_updated) > rcRegenInterval)
      {
        LogInfo("regenerating RC");
        if(!UpdateOurRC(false))
          LogError("Failed to update our RC");
      }

      /*
      // kill nodes that are not allowed by network policy
      nodedb()->RemoveIf([&](const RouterContact &rc) -> bool {
        if(IsBootstrapNode(rc.pubkey))
          return false;
        return !ConnectionToRouterAllowed(rc.pubkey);
      });
      */

      // only do this as service node
      // client endpoints do this on their own
      nodedb()->visit([&](const RouterContact &rc) -> bool {
        if(rc.ExpiresSoon(now, randint() % 10000))
          ServiceNodeLookupRouterWhenExpired(rc.pubkey);
        return true;
      });
    }
    else
    {
      // kill dead nodes if client
      nodedb()->RemoveIf([&](const RouterContact &rc) -> bool {
        // don't kill first hop nodes
        if(strictConnectPubkeys.count(rc.pubkey))
          return false;
        // don't kill "non-bad" nodes
        if(!routerProfiling().IsBad(rc.pubkey))
          return false;
        routerProfiling().ClearProfile(rc.pubkey);
        // don't kill bootstrap nodes
        return !IsBootstrapNode(rc.pubkey);
      });
    }
    // expire transit paths
    paths.ExpirePaths(now);
    {
      auto itr = pendingEstablishJobs.begin();
      while(itr != pendingEstablishJobs.end())
      {
        if(itr->second->TimeoutReached() && itr->second->Timeout())
        {
          LogWarn("failed to connect to ", itr->first);
          itr = pendingEstablishJobs.erase(itr);
        }
        else
          ++itr;
      }
    }

    {
      auto itr = m_PersistingSessions.begin();
      while(itr != m_PersistingSessions.end())
      {
        auto link = GetLinkWithSessionByPubkey(itr->first);
        if(now < itr->second)
        {
          if(link && link->HasSessionTo(itr->first))
          {
            LogDebug("keepalive to ", itr->first);
            link->KeepAliveSessionTo(itr->first);
          }
          else
          {
            RouterContact rc;
            if(nodedb()->Get(itr->first, rc))
            {
              if(rc.IsPublicRouter())
              {
                LogDebug("establish to ", itr->first);
                TryConnectAsync(rc, 5);
              }
            }
          }
          ++itr;
        }
        else
        {
          const RouterID r(itr->first);
          LogInfo("commit to ", r, " expired");
          itr = m_PersistingSessions.erase(itr);
        }
      }
    }

    const size_t connected = NumberOfConnectedRouters();
    const size_t N         = nodedb()->num_loaded();
    if(N < minRequiredRouters)
    {
      LogInfo("We need at least ", minRequiredRouters,
              " service nodes to build paths but we have ", N, " in nodedb");
      // TODO: only connect to random subset
      if(bootstrapRCList.size())
      {
        for(const auto &rc : bootstrapRCList)
        {
          dht()->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
        }
      }
      else
        LogError("we have no bootstrap nodes specified");
    }
    if(connected < minConnectedRouters)
    {
      size_t dlt = minConnectedRouters - connected;
      LogInfo("connecting to ", dlt, " random routers to keep alive");
      ConnectToRandomRouters(dlt);
    }

    if(!IsServiceNode())
    {
      _hiddenServiceContext.Tick(now);
    }

    _exitContext.Tick(now);
    if(rpcCaller)
      rpcCaller->Tick(now);
    // save profiles async
    if(routerProfiling().ShouldSave(now))
    {
      diskworker()->addJob(
          [&]() { routerProfiling().Save(routerProfilesFile.c_str()); });
    }
  }  // namespace llarp

  bool
  Router::Sign(Signature &sig, const llarp_buffer_t &buf) const
  {
    METRICS_TIME_BLOCK("Router", "Sign");
    return CryptoManager::instance()->sign(sig, identity(), buf);
  }

  void
  Router::SendTo(RouterID remote, const ILinkMessage *msg, ILinkLayer *selected)
  {
    const std::string remoteName = "TX_" + remote.ToString();
    METRICS_DYNAMIC_INCREMENT(msg->Name(), remoteName.c_str());
    llarp_buffer_t buf(linkmsg_buffer);

    if(!msg->BEncode(&buf))
    {
      LogWarn("failed to encode outbound message, buffer size left: ",
              buf.size_left());
      return;
    }
    // set size of message
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    LogDebug("send ", buf.sz, " bytes to ", remote);
    if(selected)
    {
      if(selected->SendTo(remote, buf))
        return;
    }
    for(const auto &link : inboundLinks)
    {
      if(link->SendTo(remote, buf))
        return;
    }
    for(const auto &link : outboundLinks)
    {
      if(link->SendTo(remote, buf))
        return;
    }
    LogWarn("message to ", remote, " was dropped");
  }

  void
  Router::ScheduleTicker(uint64_t ms)
  {
    ticker_job_id = _logic->call_later({ms, this, &handle_router_ticker});
  }

  void
  Router::SessionClosed(RouterID remote)
  {
    dht::Key_t k(remote);
    dht()->impl->Nodes()->DelNode(k);
    // remove from valid routers if it's a valid router
    validRouters.erase(remote);
    LogInfo("Session to ", remote, " fully closed");
  }

  ILinkLayer *
  Router::GetLinkWithSessionByPubkey(const RouterID &pubkey)
  {
    for(const auto &link : outboundLinks)
    {
      if(link->HasSessionTo(pubkey))
        return link.get();
    }
    for(const auto &link : inboundLinks)
    {
      if(link->HasSessionTo(pubkey))
        return link.get();
    }
    return nullptr;
  }

  void
  Router::FlushOutboundFor(RouterID remote, ILinkLayer *chosen)
  {
    LogDebug("Flush outbound for ", remote);

    auto itr = outboundMessageQueue.find(remote);
    if(itr == outboundMessageQueue.end())
    {
      pendingEstablishJobs.erase(remote);
      return;
    }
    // if for some reason we don't provide a link layer pick one that has it
    if(!chosen)
    {
      for(const auto &link : inboundLinks)
      {
        if(link->HasSessionTo(remote))
        {
          chosen = link.get();
          break;
        }
      }
      for(const auto &link : outboundLinks)
      {
        if(link->HasSessionTo(remote))
        {
          chosen = link.get();
          break;
        }
      }
    }
    while(itr->second.size())
    {
      llarp_buffer_t buf(itr->second.front());
      if(!chosen->SendTo(remote, buf))
        LogWarn("failed to send queued outbound message to ", remote, " via ",
                chosen->Name());

      itr->second.pop();
    }
    pendingEstablishJobs.erase(remote);
    outboundMessageQueue.erase(itr);
  }

  void
  Router::DiscardOutboundFor(const RouterID &remote)
  {
    outboundMessageQueue.erase(remote);
  }

  bool
  Router::GetRandomConnectedRouter(RouterContact &result) const
  {
    auto sz = validRouters.size();
    if(sz)
    {
      auto itr = validRouters.begin();
      if(sz > 1)
        std::advance(itr, randint() % sz);
      result = itr->second;
      return true;
    }
    return false;
  }

  bool
  Router::async_verify_RC(const RouterContact rc)
  {
    if(rc.IsPublicRouter() && whitelistRouters && IsServiceNode())
    {
      if(lokinetRouters.size() == 0)
      {
        LogError("we have no service nodes in whitelist");
        return false;
      }
      if(lokinetRouters.find(rc.pubkey) == lokinetRouters.end())
      {
        RouterID sn(rc.pubkey);
        LogInfo(sn, " is NOT a valid service node, rejecting");
        return false;
      }
    }
    if(pendingVerifyRC.count(rc.pubkey))
      return true;
    LogInfo("session with ", RouterID(rc.pubkey), " established");
    llarp_async_verify_rc *job = &pendingVerifyRC[rc.pubkey];
    async_verify_context *ctx  = new async_verify_context();
    ctx->router                = this;
    ctx->establish_job         = nullptr;

    auto itr = pendingEstablishJobs.find(rc.pubkey);
    if(itr != pendingEstablishJobs.end())
      ctx->establish_job = itr->second.get();

    job->user  = ctx;
    job->rc    = rc;
    job->valid = false;
    job->hook  = nullptr;

    job->nodedb       = _nodedb;
    job->logic        = _logic;
    job->cryptoworker = tp;
    job->diskworker   = &disk;
    if(rc.IsPublicRouter())
      job->hook = &Router::on_verify_server_rc;
    else
      job->hook = &Router::on_verify_client_rc;

    llarp_nodedb_async_verify(job);
    return true;
  }

  void
  Router::SetRouterWhitelist(const std::vector< RouterID > &routers)
  {
    lokinetRouters.clear();
    for(const auto &router : routers)
      lokinetRouters.emplace(router,
                             std::numeric_limits< llarp_time_t >::max());
    LogInfo("lokinet service node list now has ", lokinetRouters.size(),
            " routers");
  }

  bool
  Router::Run(struct llarp_nodedb *nodedb)
  {
    if(_running || _stopping)
      return false;
    this->_nodedb = nodedb;

    if(enableRPCServer)
    {
      if(rpcBindAddr.empty())
      {
        rpcBindAddr = DefaultRPCBindAddr;
      }
      rpcServer = std::make_unique< rpc::Server >(this);
      while(!rpcServer->Start(rpcBindAddr))
      {
        LogError("failed to bind jsonrpc to ", rpcBindAddr);
#if defined(ANDROID) || defined(RPI)
        sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
      }
      LogInfo("Bound RPC server to ", rpcBindAddr);
    }
    if(whitelistRouters)
    {
      rpcCaller = std::make_unique< rpc::Caller >(this);
      rpcCaller->SetAuth(lokidRPCUser, lokidRPCPassword);
      while(!rpcCaller->Start(lokidRPCAddr))
      {
        LogError("failed to start jsonrpc caller to ", lokidRPCAddr);
#if defined(ANDROID) || defined(RPI)
        sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
      }
      LogInfo("RPC Caller to ", lokidRPCAddr, " started");
    }

    llarp_threadpool_start(tp);
    disk.start();

    for(const auto &rc : bootstrapRCList)
    {
      if(this->nodedb()->Insert(rc))
      {
        LogInfo("added bootstrap node ", RouterID(rc.pubkey));
      }
      else
      {
        LogError("Failed to add bootstrap node ", RouterID(rc.pubkey));
      }
    }

    routerProfiling().Load(routerProfilesFile.c_str());

    Addr publicAddr(this->addrInfo);

    if(this->publicOverride)
    {
      LogDebug("public address:port ", publicAddr);
    }

    LogInfo("You have ", inboundLinks.size(), " inbound links");

    // set public signing key
    _rc.pubkey = seckey_topublic(identity());

    AddressInfo ai;
    for(const auto &link : inboundLinks)
    {
      if(link->GetOurAddressInfo(ai))
      {
        // override ip and port
        if(this->publicOverride)
        {
          ai.ip   = *publicAddr.addr6();
          ai.port = publicAddr.port();
        }
        if(IsBogon(ai.ip))
          continue;
        _rc.addrs.push_back(ai);
        if(ExitEnabled())
        {
          const llarp::Addr addr(ai);
          const nuint32_t a{addr.addr4()->s_addr};
          _rc.exits.emplace_back(_rc.pubkey, a);
          LogInfo(
              "Neato teh l33toh, You are a freaking exit relay. w00t!!!!! your "
              "exit "
              "is advertised as exiting at ",
              a);
        }
      }
    }

    // set public encryption key
    _rc.enckey = seckey_topublic(encryption());

    LogInfo("Signing rc...");
    if(!_rc.Sign(identity()))
    {
      LogError("failed to sign rc");
      return false;
    }

    if(!SaveRC())
    {
      LogError("failed to save RC");
      return false;
    }

    LogInfo("have ", nodedb->num_loaded(), " routers");

    LogInfo("starting outbound ", outboundLinks.size(), " links");
    for(const auto &link : outboundLinks)
    {
      if(!link->Start(_logic))
      {
        LogWarn("outbound link '", link->Name(), "' failed to start");
        return false;
      }
    }

    int IBLinksStarted = 0;

    // start links
    for(const auto &link : inboundLinks)
    {
      if(link->Start(_logic))
      {
        LogDebug("Link ", link->Name(), " started");
        IBLinksStarted++;
      }
      else
        LogWarn("Link ", link->Name(), " failed to start");
    }

    if(IBLinksStarted > 0)
    {
      // initialize as service node
      if(!InitServiceNode())
      {
        LogError("Failed to initialize service node");
        return false;
      }
      RouterID us = pubkey();
      LogInfo("initalized service node: ", us);
      if(minConnectedRouters < 6)
        minConnectedRouters = 6;
      // relays do not use profiling
      routerProfiling().Disable();
    }
    else
    {
      maxConnectedRouters = minConnectedRouters + 1;
      // we are a client
      // regenerate keys and resign rc before everything else
      CryptoManager::instance()->identity_keygen(_identity);
      CryptoManager::instance()->encryption_keygen(_encryption);
      _rc.pubkey = seckey_topublic(identity());
      _rc.enckey = seckey_topublic(encryption());
      if(!_rc.Sign(identity()))
      {
        LogError("failed to regenerate keys and sign RC");
        return false;
      }

      // don't create default if we already have some defined
      if(this->ShouldCreateDefaultHiddenService())
      {
        // generate default hidden service
        LogInfo("setting up default network endpoint");
        if(!CreateDefaultHiddenService())
        {
          LogError("failed to set up default network endpoint");
          return false;
        }
      }
    }

    LogInfo("starting hidden service context...");
    if(!hiddenServiceContext().StartAll())
    {
      LogError("Failed to start hidden service context");
      return false;
    }
    llarp_dht_context_start(dht(), pubkey());
    ScheduleTicker(1000);
    _running.store(true);
    _startedAt = Now();
    return _running;
  }

  llarp_time_t
  Router::Uptime() const
  {
    const llarp_time_t _now = Now();
    if(_startedAt && _now > _startedAt)
      return _now - _startedAt;
    return 0;
  }

  static void
  RouterAfterStopLinks(void *u, uint64_t, uint64_t)
  {
    Router *self = static_cast< Router * >(u);
    self->Close();
  }

  static void
  RouterAfterStopIssued(void *u, uint64_t, uint64_t)
  {
    Router *self = static_cast< Router * >(u);
    self->StopLinks();
    self->_logic->call_later({200, self, &RouterAfterStopLinks});
  }

  void
  Router::StopLinks()
  {
    LogInfo("stopping links");
    for(const auto &link : outboundLinks)
      link->Stop();
    for(const auto &link : inboundLinks)
      link->Stop();
  }

  bool
  Router::ShouldCreateDefaultHiddenService()
  {
    std::string defaultIfAddr = "auto";
    std::string defaultIfName = "auto";
    std::string enabledOption = "auto";
    auto itr                  = netConfig.find("defaultIfAddr");
    if(itr != netConfig.end())
    {
      defaultIfAddr = itr->second;
    }
    itr = netConfig.find("defaultIfName");
    if(itr != netConfig.end())
    {
      defaultIfName = itr->second;
    }
    itr = netConfig.find("enabled");
    if(itr != netConfig.end())
    {
      enabledOption = itr->second;
    }
    LogDebug("IfName: ", defaultIfName, " IfAddr: ", defaultIfAddr,
             " Enabled: ", enabledOption);
    // LogInfo("IfAddr: ", itr->second);
    // LogInfo("IfName: ", itr->second);
    if(enabledOption == "false")
    {
      LogInfo("Disabling default hidden service");
      return false;
    }
    else if(enabledOption == "auto")
    {
      // auto detect if we have any pre-defined endpoints
      // no if we have a endpoints
      if(hiddenServiceContext().hasEndpoints())
      {
        LogInfo("Auto mode detected and we have endpoints");
        netConfig.emplace("enabled", "false");
        return false;
      }
      netConfig.emplace("enabled", "true");
    }
    // ev.cpp llarp_ev_add_tun now handles this
    /*
    // so basically enabled at this point
    if(defaultIfName == "auto")
    {
      // we don't have any endpoints, auto configure settings
      // set a default IP range
      defaultIfAddr = findFreePrivateRange();
      if(defaultIfAddr == "")
      {
        LogError(
                        "Could not find any free lokitun interface names, can't
    auto set up " "default HS context for client"); defaultIfAddr = "no";
        netConfig.emplace("defaultIfAddr", defaultIfAddr);
        return false;
      }
      netConfig.emplace("defaultIfAddr", defaultIfAddr);
    }
    if(defaultIfName == "auto")
    {
      // pick an ifName
      defaultIfName = findFreeLokiTunIfName();
      if(defaultIfName == "")
      {
        LogError(
                        "Could not find any free private ip ranges, can't auto
    set up " "default HS context for client"); defaultIfName = "no";
        netConfig.emplace("defaultIfName", defaultIfName);
        return false;
      }
      netConfig.emplace("defaultIfName", defaultIfName);
    }
    */
    return true;
  }

  void
  Router::Stop()
  {
    if(!_running)
      return;
    if(_stopping)
      return;

    _stopping.store(true);
    LogInfo("stopping router");
    hiddenServiceContext().StopAll();
    _exitContext.Stop();
    if(rpcServer)
      rpcServer->Stop();
    _logic->call_later({200, this, &RouterAfterStopIssued});
  }

  bool
  Router::HasSessionTo(const RouterID &remote) const
  {
    for(const auto &link : outboundLinks)
      if(link->HasSessionTo(remote))
        return true;
    for(const auto &link : inboundLinks)
      if(link->HasSessionTo(remote))
        return true;
    return false;
  }

  void
  Router::ConnectToRandomRouters(int want)
  {
    int wanted   = want;
    Router *self = this;

    self->nodedb()->visit([self, &want](const RouterContact &other) -> bool {
      // check if we really want to
      if(other.ExpiresSoon(self->Now(), 30000))
        return want > 0;
      if(!self->ConnectionToRouterAllowed(other.pubkey))
        return want > 0;
      if(randint() % 2 == 0
         && !(self->HasSessionTo(other.pubkey)
              || self->HasPendingConnectJob(other.pubkey)))
      {
        if(self->TryConnectAsync(other, 5))
          --want;
      }
      return want > 0;
    });
    LogInfo("connecting to ", abs(want - wanted), " out of ", wanted,
            " random routers");
  }

  bool
  Router::InitServiceNode()
  {
    LogInfo("accepting transit traffic");
    paths.AllowTransit();
    llarp_dht_allow_transit(dht());
    return _exitContext.AddExitEndpoint("default-connectivity", netConfig);
  }

  /// validate a new configuration against an already made and running
  /// router
  struct RouterConfigValidator
  {
    void
    ValidateEntry(const char *section, const char *key, const char *val)
    {
      if(valid)
      {
        if(!OnEntry(section, key, val))
        {
          LogError("invalid entry in section [", section, "]: '", key, "'='",
                   val, "'");
          valid = false;
        }
      }
    }

    const Router *router;
    Config *config;
    bool valid;
    RouterConfigValidator(const Router *r, Config *conf)
        : router(r), config(conf), valid(true)
    {
    }

    /// checks the (section, key, value) config tuple
    /// return false if that entry conflicts
    /// with existing configuration in router
    bool
    OnEntry(const char *, const char *, const char *) const
    {
      // TODO: implement me
      return true;
    }

    /// do validation
    /// return true if this config is valid
    /// return false if this config is not valid
    bool
    Validate()
    {
      config->visit(util::memFn(&RouterConfigValidator::ValidateEntry, this));
      return valid;
    }
  };

  bool
  Router::ValidateConfig(Config *conf) const
  {
    RouterConfigValidator validator(this, conf);
    return validator.Validate();
  }

  bool
  Router::Reconfigure(Config *)
  {
    // TODO: implement me
    return true;
  }

  bool
  Router::InitOutboundLinks()
  {
    if(outboundLinks.size() > 0)
      return true;

    static std::list< std::function< LinkLayer_ptr(Router *) > > linkFactories =
        {utp::NewServerFromRouter, iwp::NewServerFromRouter};

    for(const auto &factory : linkFactories)
    {
      auto link = factory(this);
      if(!link)
        continue;
      if(!link->EnsureKeys(transport_keyfile.string().c_str()))
      {
        LogError("failed to load ", transport_keyfile);
        continue;
      }

      auto afs = {AF_INET, AF_INET6};

      for(auto af : afs)
      {
        if(!link->Configure(netloop(), "*", af, m_OutboundPort))
          continue;
        AddLink(std::move(link), false);
        break;
      }
    }
    return outboundLinks.size() > 0;
  }

  bool
  Router::CreateDefaultHiddenService()
  {
    // fallback defaults
    // To NeuroScr: why run findFree* here instead of in tun.cpp?
    // I think it should be in tun.cpp, better to closer to time of usage
    // that way new tun may have grab a range we may have also grabbed here
    static const std::unordered_map< std::string,
                                     std::function< std::string(void) > >
        netConfigDefaults = {
            {"ifname", []() -> std::string { return "auto"; }},
            {"ifaddr", []() -> std::string { return "auto"; }},
            {"local-dns", []() -> std::string { return "127.0.0.1:53"; }}};
    // populate with fallback defaults if values not present
    auto itr = netConfigDefaults.begin();
    while(itr != netConfigDefaults.end())
    {
      auto found = netConfig.find(itr->first);
      if(found == netConfig.end() || found->second.empty())
      {
        netConfig.emplace(itr->first, itr->second());
      }
      ++itr;
    }
    // add endpoint
    return hiddenServiceContext().AddDefaultEndpoint(netConfig);
  }

  bool
  Router::HasPendingConnectJob(const RouterID &remote)
  {
    return pendingEstablishJobs.find(remote) != pendingEstablishJobs.end();
  }

  bool
  Router::LoadHiddenServiceConfig(const char *fname)
  {
    LogDebug("opening hidden service config ", fname);
    service::Config conf;
    if(!conf.Load(fname))
      return false;
    for(const auto &config : conf.services)
    {
      service::Config::section_t filteredConfig;
      mergeHiddenServiceConfig(config.second, filteredConfig.second);
      filteredConfig.first = config.first;
      if(!hiddenServiceContext().AddEndpoint(filteredConfig))
        return false;
    }
    return true;
  }
}  // namespace llarp

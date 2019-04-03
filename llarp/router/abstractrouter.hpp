#ifndef LLARP_ABSTRACT_ROUTER_HPP
#define LLARP_ABSTRACT_ROUTER_HPP

#include <util/types.hpp>
#include <util/status.hpp>
#include <vector>

struct llarp_buffer_t;
struct llarp_dht_context;
struct llarp_ev_loop;
struct llarp_nodedb;
struct llarp_threadpool;

namespace llarp
{
  class Logic;
  struct Config;
  struct Crypto;
  struct RouterContact;
  struct RouterID;
  struct ILinkMessage;
  struct ILinkSession;
  struct PathID_t;
  struct Profiling;
  struct SecretKey;
  struct Signature;

  namespace exit
  {
    struct Context;
  }

  namespace path
  {
    struct PathContext;
  }

  namespace routing
  {
    struct IMessageHandler;
  }

  namespace service
  {
    struct Context;
  }

  struct AbstractRouter : public util::IStateful
  {
    virtual bool
    OnSessionEstablished(ILinkSession *) = 0;

    virtual bool
    HandleRecvLinkMessageBuffer(ILinkSession *from,
                                const llarp_buffer_t &msg) = 0;

    virtual Logic *
    logic() const = 0;

    virtual llarp_dht_context *
    dht() const = 0;

    virtual Crypto *
    crypto() const = 0;

    virtual llarp_nodedb *
    nodedb() const = 0;

    virtual const path::PathContext &
    pathContext() const = 0;

    virtual path::PathContext &
    pathContext() = 0;

    virtual const RouterContact &
    rc() const = 0;

    virtual exit::Context &
    exitContext() = 0;

    virtual const SecretKey &
    identity() const = 0;

    virtual const SecretKey &
    encryption() const = 0;

    virtual Profiling &
    routerProfiling() = 0;

    virtual llarp_ev_loop *
    netloop() const = 0;

    virtual llarp_threadpool *
    threadpool() = 0;

    virtual llarp_threadpool *
    diskworker() = 0;

    virtual service::Context &
    hiddenServiceContext() = 0;

    virtual const service::Context &
    hiddenServiceContext() const = 0;

    virtual bool
    Sign(Signature &sig, const llarp_buffer_t &buf) const = 0;

    virtual bool
    Configure(Config *conf) = 0;

    virtual bool
    Run(struct llarp_nodedb *nodedb) = 0;

    /// stop running the router logic gracefully
    virtual void
    Stop() = 0;

    virtual bool
    IsBootstrapNode(RouterID r) const = 0;
    
    virtual const byte_t *
    pubkey() const = 0;

    virtual void
    OnConnectTimeout(ILinkSession *session) = 0;

    /// connect to N random routers
    virtual void
    ConnectToRandomRouters(int N) = 0;
    /// inject configuration and reconfigure router
    virtual bool
    Reconfigure(Config *conf) = 0;

    /// validate new configuration against old one
    /// return true on 100% valid
    /// return false if not 100% valid
    virtual bool
    ValidateConfig(Config *conf) const = 0;

    /// called by link when a remote session has no more sessions open
    virtual void
    SessionClosed(RouterID remote) = 0;

    virtual llarp_time_t
    Now() const = 0;

    virtual bool
    GetRandomGoodRouter(RouterID &r) = 0;

    virtual bool
    SendToOrQueue(const RouterID &remote, const ILinkMessage *msg) = 0;

    virtual void
    PersistSessionUntil(const RouterID &remote, llarp_time_t until) = 0;

    virtual bool
    ParseRoutingMessageBuffer(const llarp_buffer_t &buf,
                              routing::IMessageHandler *h,
                              const PathID_t &rxid) = 0;

    virtual size_t
    NumberOfConnectedRouters() const = 0;

    virtual bool
    GetRandomConnectedRouter(RouterContact &result) const = 0;

    virtual void
    HandleDHTLookupForExplore(RouterID remote,
                              const std::vector< RouterContact > &results) = 0;

    /// lookup router by pubkey
    /// if we are a service node this is done direct otherwise it's done via
    /// path
    virtual void
    LookupRouter(RouterID remote) = 0;

    /// check if newRc matches oldRC and update local rc for this remote contact
    /// if valid
    /// returns true on valid and updated
    /// returns false otherwise
    virtual bool
    CheckRenegotiateValid(RouterContact newRc, RouterContact oldRC) = 0;

    /// set router's service node whitelist
    virtual void
    SetRouterWhitelist(const std::vector< RouterID > &routers) = 0;

    /// visit each connected link session
    virtual void
    ForEachPeer(
      std::function< void(const ILinkSession *, bool) > visit, bool randomize) const = 0;
  };
}  // namespace llarp

#endif

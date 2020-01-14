#ifndef LLARP_I_RC_LOOKUP_HANDLER_HPP
#define LLARP_I_RC_LOOKUP_HANDLER_HPP

#include <util/types.hpp>
#include <router_id.hpp>

#include <memory>
#include <set>
#include <vector>

namespace llarp
{
  struct RouterContact;

  enum class RCRequestResult
  {
    Success,
    InvalidRouter,
    RouterNotFound,
    BadRC
  };

  using RCRequestCallback = std::function< void(
      const RouterID &, const RouterContact *const, const RCRequestResult) >;

  struct I_RCLookupHandler
  {
    virtual ~I_RCLookupHandler() = default;

    virtual void
    AddValidRouter(const RouterID &router) = 0;

    virtual void
    RemoveValidRouter(const RouterID &router) = 0;

    virtual void
    SetRouterWhitelist(const std::vector< RouterID > &routers) = 0;

    virtual void
    GetRC(const RouterID &router, RCRequestCallback callback,
          bool forceLookup = false) = 0;

    virtual bool
    RemoteIsAllowed(const RouterID &remote) const = 0;

    virtual bool
    CheckRC(const RouterContact &rc) const = 0;

    virtual bool
    GetRandomWhitelistRouter(RouterID &router) const = 0;

    virtual bool
    CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc) = 0;

    virtual void
    PeriodicUpdate(llarp_time_t now) = 0;

    virtual void
    ExploreNetwork() = 0;

    virtual size_t
    NumberOfStrictConnectRouters() const = 0;
  };

}  // namespace llarp

#endif  // LLARP_I_RC_LOOKUP_HANDLER_HPP

#ifndef LLARP_EXIT_SESSION_HPP
#define LLARP_EXIT_SESSION_HPP
#include <llarp/pathbuilder.hpp>

namespace llarp
{
  namespace exit
  {
    /// a persisiting exit session with an exit router
    struct BaseSession : public llarp::path::Builder
    {
      BaseSession(const llarp::RouterID& exitRouter, llarp_router* r,
                  size_t numpaths, size_t hoplen);

      ~BaseSession();

      bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop) override;

     protected:
      llarp::RouterID m_ExitRouter;
    };

    /// a N-hop exit sesssion form a client
    struct ClientSesssion final : public BaseSession
    {
    };

    /// a "direct" session between service nodes
    struct DirectSession final : public BaseSession
    {
    };

  }  // namespace exit
}  // namespace llarp

#endif
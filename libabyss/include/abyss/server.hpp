#ifndef __ABYSS_SERVER_HPP__
#define __ABYSS_SERVER_HPP__

#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/time.h>
#include <list>
#include <memory>
#include <string>
#include <abyss/json.hpp>

namespace abyss
{
  namespace http
  {
    struct ConnImpl;

    struct IRPCHandler
    {
      typedef std::string Method_t;
      typedef abyss::json::Object Params;
      typedef abyss::json::Object Response;

      IRPCHandler(ConnImpl* impl);

      virtual bool
      HandleJSONRPC(const Method_t& method, const Params& params,
                    Response& response) = 0;

      ~IRPCHandler();

      bool
      ShouldClose(llarp_time_t now) const;

     private:
      ConnImpl* m_Impl;
    };

    struct BaseReqHandler
    {
      BaseReqHandler(llarp_time_t req_timeout);
      ~BaseReqHandler();

      bool
      ServeAsync(llarp_ev_loop* loop, llarp_logic* logic,
                 const sockaddr* bindaddr);

      void
      RemoveConn(IRPCHandler* handler);

     protected:
      virtual IRPCHandler*
      CreateHandler(ConnImpl* connimpl) const = 0;

     private:
      static void
      OnTick(llarp_tcp_acceptor*);

      void
      Tick();

      static void
      OnAccept(struct llarp_tcp_acceptor*, struct llarp_tcp_conn*);

      llarp_ev_loop* m_loop;
      llarp_logic* m_Logic;
      llarp_tcp_acceptor m_acceptor;
      std::list< std::unique_ptr< IRPCHandler > > m_Conns;
      llarp_time_t m_ReqTimeout;
    };
  }  // namespace http
}  // namespace abyss

#endif

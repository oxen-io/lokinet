#ifndef __LIB_ABYSS_HPP__
#define __LIB_ABYSS_HPP__

#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/time.h>
#include <vector>
#include <memory>

namespace abyss
{
  namespace http
  {
    // forward declare
    struct ConnHandler;

    struct BaseReqHandler
    {
      BaseReqHandler(llarp_time_t req_timeout);
      ~BaseReqHandler();

      bool
      ServeAsync(llarp_ev_loop* loop, llarp_logic* logic,
                 const sockaddr* bindaddr);

     private:
      static void
      OnAccept(struct llarp_tcp_acceptor*, struct llarp_tcp_conn*);

      llarp_ev_loop* m_loop;
      llarp_logic* m_Logic;
      llarp_tcp_acceptor m_acceptor;
      std::vector< std::unique_ptr< ConnHandler > > m_Conns;
      llarp_time_t m_ReqTimeout;
    };
  }  // namespace http
}  // namespace abyss

#endif

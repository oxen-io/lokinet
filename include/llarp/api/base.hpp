#ifndef LLARP_API_BASE_HPP
#define LLARP_API_BASE_HPP
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/threadpool.h>
#include <llarp/api/messages.hpp>
#include <llarp/api/parser.hpp>

namespace llarp
{
  namespace api
  {
    template < typename Handler >
    struct Base
    {
      Base(llarp_ev_loop* evloop, Handler* handler) : m_Handler(handler)
      {
        loop   = evloop;
        worker = llarp_init_same_process_threadpool();
        logic  = llarp_init_single_process_logic(worker);
      }

      static void
      HandleRecv(llarp_udp_io* u, const sockaddr* from, const void* buf,
                 ssize_t sz)
      {
        static_cast< Base< Handler >* >(u->user)->RecvFrom(from, buf, sz);
      }

      void
      RecvFrom(const sockaddr* from, const void* b, ssize_t sz)
      {
        if(from->sa_family != AF_INET
           || ((sockaddr_in*)from)->sin_addr.s_addr != apiAddr.sin_addr.s_addr
           || ((sockaddr_in*)from)->sin_port != apiAddr.sin_port)
        {
          // address missmatch
          llarp::LogWarn("got packet from bad address");
          return;
        }
        llarp_buffer_t buf;
        buf.base      = (byte_t*)b;
        buf.cur       = buf.base;
        buf.sz        = sz;
        IMessage* msg = m_MessageParser.ParseMessage(buf);
        if(msg)
        {
          m_Handler->HandleMessage(msg);
          delete msg;
        }
        else
          llarp::LogWarn("Got Invalid Message");
      }

      bool
      BindDefault()
      {
        return BindAddress(INADDR_LOOPBACK, 0);
      }

      bool
      BindAddress(in_addr_t addr, in_port_t port)
      {
        ouraddr.sin_family      = AF_INET;
        ouraddr.sin_addr.s_addr = htonl(addr);
        ouraddr.sin_port        = htons(port);
        udp.user                = this;
        udp.recvfrom            = &HandleRecv;
        return llarp_ev_add_udp(loop, &udp, (const sockaddr*)&ouraddr) != -1;
      }

      bool
      SendMessage(const IMessage* msg)
      {
        llarp_buffer_t buf;
        byte_t tmp[1500];
        buf.base = tmp;
        buf.cur  = buf.base;
        buf.sz   = sizeof(tmp);
        if(msg->BEncode(&buf))
        {
          buf.sz = buf.cur - buf.base;
          return llarp_ev_udp_sendto(&udp, (const sockaddr*)&apiAddr, buf.base,
                                     buf.sz)
              != -1;
        }
        llarp::LogError("Failed to encode message");
        return false;
      }

      int
      Mainloop()
      {
        llarp_ev_loop_run_single_process(loop, worker, logic);
        return 0;
      }

      llarp_threadpool* worker;
      llarp_logic* logic;
      llarp_ev_loop* loop;
      sockaddr_in ouraddr;
      sockaddr_in apiAddr;
      llarp_udp_io udp;
      MessageParser m_MessageParser;
      Handler* m_Handler;
    };
  }  // namespace api
}  // namespace llarp

#endif
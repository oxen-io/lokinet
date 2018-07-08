#include <arpa/inet.h>
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <llarp/api/client.hpp>
#include <llarp/api/messages.hpp>
#include <llarp/api/parser.hpp>

namespace llarp
{
  namespace api
  {
    struct ClientPImpl
    {
      ClientPImpl()
      {
        llarp_ev_loop_alloc(&loop);
        worker = llarp_init_same_process_threadpool();
        logic  = llarp_init_single_process_logic(worker);
      }

      ~ClientPImpl()
      {
        llarp_ev_loop_free(&loop);
      }

      static void
      HandleRecv(llarp_udp_io* u, const sockaddr* from, const void* buf,
                 ssize_t sz)
      {
        static_cast< ClientPImpl* >(u->user)->RecvFrom(from, buf, sz);
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
          delete msg;
        }
        else
          llarp::LogWarn("Got Invalid Message");
      }

      bool
      BindDefault()
      {
        ouraddr.sin_family      = AF_INET;
        ouraddr.sin_addr.s_addr = INADDR_LOOPBACK;
        ouraddr.sin_port        = 0;
        udp.user                = this;
        udp.recvfrom            = &HandleRecv;
        return llarp_ev_add_udp(loop, &udp, (const sockaddr*)&ouraddr) != -1;
      }

      bool
      StartSession(const std::string& addr, uint16_t port)
      {
        apiAddr.sin_family = AF_INET;
        if(inet_pton(AF_INET, addr.c_str(), &apiAddr.sin_addr.s_addr) == -1)
          return false;
        apiAddr.sin_port = htons(port);
        CreateSessionMessage msg;
        return SendMessage(&msg);
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
          return llarp_ev_udp_sendto(&udp, (const sockaddr*)&apiAddr, buf.base,
                                     buf.sz)
              != -1;
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
    };

    Client::Client() : m_Impl(new ClientPImpl)
    {
    }

    Client::~Client()
    {
      delete m_Impl;
    }

    bool
    Client::Start(const std::string& url)
    {
      if(url.find(":") == std::string::npos)
        return false;
      if(!m_Impl->BindDefault())
        return false;
      std::string addr    = url.substr(0, url.find(":"));
      std::string strport = url.substr(url.find(":") + 1);
      int port            = std::stoi(strport);
      if(port == -1)
        return false;
      return m_Impl->StartSession(addr, port);
    }

    int
    Client::Mainloop()
    {
      return m_Impl->Mainloop();
    }

  }  // namespace api
}  // namespace llarp
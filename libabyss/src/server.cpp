#include <libabyss.hpp>
#include <llarp/time.h>
#include <sstream>
#include <unordered_map>
#include <string_view>

namespace abyss
{
  namespace http
  {
    struct RequestHeader
    {
      typedef std::unordered_multimap< std::string, std::string > Headers_t;
      Headers_t m_Headers;
      std::string Method;
      std::string Version;
    };

    struct ConnImpl
    {
      llarp_tcp_conn* _conn;
      IRPCHandler* handler;
      BaseReqHandler* _parent;
      llarp_time_t m_LastActive;
      llarp_time_t m_ReadTimeout;
      bool m_Bad;
      RequestHeader m_Header;
      std::stringstream m_ReadBuf;

      enum HTTPState
      {
        eReadHTTPMethodLine,
        eReadHTTPHeaders,
        eReadHTTPBody,
        eWriteHTTPStatusLine,
        eWriteHTTPHeaders,
        eWriteHTTPBody,
        eCloseMe
      };

      HTTPState m_State;

      ConnImpl(BaseReqHandler* p, llarp_tcp_conn* c, llarp_time_t readtimeout)
          : _conn(c), _parent(p)
      {
        handler       = nullptr;
        m_LastActive  = llarp_time_now_ms();
        m_ReadTimeout = readtimeout;
        c->read       = &OnRead;
        c->tick       = &OnTick;
        c->closed     = nullptr;
        m_Bad         = false;
        m_State       = eReadHTTPMethodLine;
      }

      ~ConnImpl()
      {
      }

      bool
      FeedLine(const std::string& line)
      {
        return false;
      }

      bool
      ProcessRead(const char* buf, size_t sz)
      {
        if(m_Bad)
          return false;

        m_ReadBuf << std::string_view(buf, sz);
        return true;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const void* buf, size_t sz)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        if(!self->ProcessRead((const char*)buf, sz))
          self->MarkBad();
      }

      static void
      OnTick(llarp_tcp_conn* conn)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        self->Tick();
      }

      void
      Tick()
      {
      }

      /// mark bad so next tick we are closed
      void
      MarkBad()
      {
        m_Bad = true;
      }

      bool
      ShouldClose(llarp_time_t now) const
      {
        return now - m_LastActive > m_ReadTimeout || m_Bad;
      }

      void
      Close()
      {
        llarp_tcp_conn_close(_conn);
      }
    };

    IRPCHandler::IRPCHandler(ConnImpl* conn) : m_Impl(conn)
    {
    }

    IRPCHandler::~IRPCHandler()
    {
      m_Impl->Close();
      delete m_Impl;
    }

    bool
    IRPCHandler::ShouldClose(llarp_time_t now) const
    {
      return m_Impl->ShouldClose(now);
    }

    BaseReqHandler::BaseReqHandler(llarp_time_t reqtimeout)
        : m_ReqTimeout(reqtimeout)
    {
      m_loop              = nullptr;
      m_Logic             = nullptr;
      m_acceptor.accepted = &BaseReqHandler::OnAccept;
      m_acceptor.user     = this;
    }

    bool
    BaseReqHandler::ServeAsync(llarp_ev_loop* loop, llarp_logic* logic,
                               const sockaddr* bindaddr)
    {
      m_loop  = loop;
      m_Logic = logic;
      if(!llarp_tcp_serve(m_loop, &m_acceptor, bindaddr))
        return false;
      ScheduleTick(1000);
      return true;
    }

    void
    BaseReqHandler::OnTick(void* user, llarp_time_t orig, llarp_time_t left)
    {
      if(left)
        return;
      BaseReqHandler* self = static_cast< BaseReqHandler* >(user);

      self->Tick();

      self->ScheduleTick(orig);
    }

    void
    BaseReqHandler::Tick()
    {
      auto now = llarp_time_now_ms();
      auto itr = m_Conns.begin();
      while(itr != m_Conns.end())
      {
        if((*itr)->ShouldClose(now))
          itr = m_Conns.erase(itr);
        else
          ++itr;
      }
    }

    void
    BaseReqHandler::ScheduleTick(llarp_time_t timeout)
    {
      llarp_logic_call_later(m_Logic, {timeout, this, &BaseReqHandler::OnTick});
    }

    BaseReqHandler::~BaseReqHandler()
    {
      llarp_tcp_acceptor_close(&m_acceptor);
    }

    void
    BaseReqHandler::OnAccept(llarp_tcp_acceptor* acceptor, llarp_tcp_conn* conn)
    {
      BaseReqHandler* self    = static_cast< BaseReqHandler* >(acceptor->user);
      ConnImpl* connimpl      = new ConnImpl(self, conn, self->m_ReqTimeout);
      IRPCHandler* rpcHandler = self->CreateHandler(connimpl);
      if(rpcHandler == nullptr)
      {
        connimpl->Close();
        delete connimpl;
        return;
      }
      connimpl->handler = rpcHandler;
      conn->user        = connimpl;
      self->m_Conns.emplace_back(rpcHandler);
    }
  }  // namespace http
}  // namespace abyss

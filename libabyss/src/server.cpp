#include <libabyss.hpp>
#include <llarp/time.h>

namespace abyss
{
  namespace http
  {
    struct ConnHandler
    {
      llarp_tcp_conn* _conn;

      llarp_time_t m_LastActive;
      llarp_time_t m_ReadTimeout;

      ConnHandler(llarp_tcp_conn* c, llarp_time_t readtimeout) : _conn(c)
      {
        m_LastActive  = llarp_time_now_ms();
        m_ReadTimeout = readtimeout;
      }

      bool
      ShouldClose(llarp_time_t now) const
      {
        return now - m_LastActive > m_ReadTimeout;
      }

      void
      Begin()
      {
      }
    };

    BaseReqHandler::BaseReqHandler(llarp_time_t reqtimeout)
        : m_ReqTimeout(reqtimeout)
    {
      m_loop              = nullptr;
      m_Logic             = nullptr;
      m_acceptor.accepted = &BaseReqHandler::OnAccept;
      m_acceptor.user     = this;
    }

    BaseReqHandler::~BaseReqHandler()
    {
      llarp_tcp_acceptor_close(&m_acceptor);
    }

    void
    BaseReqHandler::OnAccept(llarp_tcp_acceptor* acceptor, llarp_tcp_conn* conn)
    {
      BaseReqHandler* self = static_cast< BaseReqHandler* >(acceptor->user);
      ConnHandler* handler = new ConnHandler(conn, self->m_ReqTimeout);
      conn->user           = handler;
      self->m_Conns.emplace_back(handler);
    }
  }  // namespace http
}  // namespace abyss

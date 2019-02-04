#include <abyss/client.hpp>
#include <abyss/http.hpp>
#include <crypto/crypto.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>

namespace abyss
{
  namespace http
  {
    struct ConnImpl : HeaderReader
    {
      // big
      static const size_t MAX_BODY_SIZE = (1024 * 1024);
      llarp_tcp_conn* m_Conn;
      json::Document m_RequestBody;
      Headers_t m_SendHeaders;
      IRPCClientHandler* handler;
      std::unique_ptr< abyss::json::IParser > m_BodyParser;
      json::Document m_Response;

      enum State
      {
        eInitial,
        eReadStatusLine,
        eReadResponseHeader,
        eReadResponseBody,
        eCloseMe
      };

      State state;

      ConnImpl(llarp_tcp_conn* conn, RPC_Method_t method, RPC_Params params,
               JSONRPC::HandlerFactory factory)
          : m_Conn(conn), state(eInitial)
      {
        srand(time(nullptr));
        conn->user   = this;
        conn->closed = &ConnImpl::OnClosed;
        conn->read   = &ConnImpl::OnRead;
        conn->tick   = &ConnImpl::OnTick;

        handler = factory(this);

        m_RequestBody.SetObject();
        auto& alloc = m_RequestBody.GetAllocator();
        m_RequestBody.AddMember("jsonrpc", json::Value().SetString("2.0"),
                                alloc);
        m_RequestBody.AddMember("id", json::Value(abs(rand())), alloc);
        m_RequestBody.AddMember(
            "method", json::Value().SetString(method.c_str(), alloc), alloc);
        m_RequestBody.AddMember("params", params, alloc);
      }

      static void
      OnClosed(llarp_tcp_conn* conn)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        self->state    = eCloseMe;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const llarp_buffer_t& buf)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        if(!self->ProcessRead((const char*)buf.base, buf.sz))
        {
          self->CloseError();
        }
      }

      static void
      OnTick(__attribute__((unused)) llarp_tcp_conn* conn)
      {
      }

      bool
      ProcessStatusLine(string_view line) const
      {
        auto idx = line.find_first_of(' ');
        if(idx == string_view::npos)
          return false;

        string_view codePart = line.substr(1 + idx);
        idx                  = codePart.find_first_of(' ');

        if(idx == string_view::npos)
          return false;
        return HandleStatusCode(codePart.substr(0, idx));
      }

      bool
      ShouldProcessHeader(const abyss::string_view& name) const
      {
        return name == "content-length" || name == "content-type";
      }

      /// return true if we get a 200 status code
      bool
      HandleStatusCode(string_view code) const
      {
        return code == "200";
      }

      bool
      ProcessBody(const char* buf, size_t sz)
      {
        // init parser
        if(m_BodyParser == nullptr)
        {
          size_t contentSize = 0;
          auto itr           = Header.Headers.find("content-length");
          // no content-length header
          if(itr == Header.Headers.end())
            return false;

          // check size
          contentSize = std::stoul(itr->second);
          if(contentSize > MAX_BODY_SIZE)
            return false;

          m_BodyParser.reset(abyss::json::MakeParser(contentSize));
        }
        if(m_BodyParser && m_BodyParser->FeedData(buf, sz))
        {
          switch(m_BodyParser->Parse(m_Response))
          {
            case json::IParser::eNeedData:
              return true;
            case json::IParser::eDone:
              handler->HandleResponse(std::move(m_Response));
              Close();
              return true;
            case json::IParser::eParseError:
              handler->HandleError();
              return false;
            default:
              return false;
          }
        }
        else
          return false;
      }

      bool
      ProcessRead(const char* buf, size_t sz)
      {
        if(state == eInitial)
          return true;
        if(!sz)
          return true;
        bool done = false;
        while(state < eReadResponseBody)
        {
          const char* end = strstr(buf, "\r\n");
          if(!end)
            return false;
          string_view line(buf, end - buf);
          switch(state)
          {
            case eReadStatusLine:
              if(!ProcessStatusLine(line))
                return false;
              sz -= line.size() + (2 * sizeof(char));
              state = eReadResponseHeader;
              break;
            case eReadResponseHeader:
              if(!ProcessHeaderLine(line, done))
                return false;
              sz -= line.size() + (2 * sizeof(char));
              if(done)
                state = eReadResponseBody;
              break;
            default:
              break;
          }
          buf = end + (2 * sizeof(char));
          end = strstr(buf, "\r\n");
        }
        if(state == eReadResponseBody)
          return ProcessBody(buf, sz);
        return state == eCloseMe;
      }

      bool
      ShouldClose() const
      {
        return state == eCloseMe;
      }

      void
      CloseError()
      {
        if(handler)
          handler->HandleError();
        handler = nullptr;
        Close();
      }

      void
      Close()
      {
        if(m_Conn)
          llarp_tcp_conn_close(m_Conn);
        m_Conn = nullptr;
      }

      void
      SendRequest()
      {
        // populate request headers
        handler->PopulateReqHeaders(m_SendHeaders);
        // create request body
        std::string body;
        json::ToString(m_RequestBody, body);
        // request base
        char buf[512] = {0};
        int sz        = snprintf(buf, sizeof(buf),
                          "POST /rpc HTTP/1.0\r\nContent-Type: "
                          "application/json\r\nContent-Length: %zu\r\nAccept: "
                          "application/json\r\n",
                          body.size());
        if(sz <= 0)
          return;
        if(!llarp_tcp_conn_async_write(m_Conn, llarp_buffer_t(buf, sz)))
        {
          llarp::LogError("failed to write first part of request");
          CloseError();
          return;
        }
        // header delimiter
        buf[0] = ':';
        buf[1] = ' ';
        // CRLF
        buf[2] = '\r';
        buf[3] = '\n';
        // write extra request header
        for(const auto& item : m_SendHeaders)
        {
          // header name
          if(!llarp_tcp_conn_async_write(
                 m_Conn, llarp_buffer_t(item.first.c_str(), item.first.size())))
          {
            CloseError();
            return;
          }
          // header delimiter
          if(!llarp_tcp_conn_async_write(m_Conn,
                                         llarp_buffer_t(buf, 2 * sizeof(char))))
          {
            CloseError();
            return;
          }
          // header value
          if(!llarp_tcp_conn_async_write(
                 m_Conn,
                 llarp_buffer_t(item.second.c_str(), item.second.size())))
          {
            CloseError();
            return;
          }
          // CRLF
          if(!llarp_tcp_conn_async_write(
                 m_Conn, llarp_buffer_t(buf + 2, 2 * sizeof(char))))
          {
            CloseError();
            return;
          }
        }
        // CRLF
        if(!llarp_tcp_conn_async_write(
               m_Conn, llarp_buffer_t(buf + 2, 2 * sizeof(char))))
        {
          CloseError();
          return;
        }
        // request body
        if(!llarp_tcp_conn_async_write(
               m_Conn, llarp_buffer_t(body.c_str(), body.size())))
        {
          CloseError();
          return;
        }
        llarp::LogDebug("request sent");
        state = eReadStatusLine;
      }
    };

    void
    JSONRPC::Flush()
    {
      /// close idle connections
      auto itr = m_Conns.begin();
      while(itr != m_Conns.end())
      {
        if((*itr)->ShouldClose())
        {
          (*itr)->Close();
          itr = m_Conns.erase(itr);
        }
        else
          ++itr;
      }
      // open at most 10 connections
      size_t numCalls = std::min(m_PendingCalls.size(), (size_t)10UL);
      llarp::LogDebug("tick connect to rpc ", numCalls, " times");
      while(numCalls--)
      {
        llarp_tcp_async_try_connect(m_Loop, &m_connect);
      }
    }

    IRPCClientHandler::IRPCClientHandler(ConnImpl* impl) : m_Impl(impl)
    {
    }

    bool
    IRPCClientHandler::ShouldClose() const
    {
      return m_Impl && m_Impl->ShouldClose();
    }

    void
    IRPCClientHandler::Close() const
    {
      if(m_Impl)
        m_Impl->Close();
    }

    IRPCClientHandler::~IRPCClientHandler()
    {
      if(m_Impl)
        delete m_Impl;
    }

    JSONRPC::JSONRPC()
    {
      m_Run.store(true);
    }

    JSONRPC::~JSONRPC()
    {
    }

    void
    JSONRPC::QueueRPC(RPC_Method_t method, RPC_Params params,
                      HandlerFactory createHandler)
    {
      if(m_Run)
        m_PendingCalls.emplace_back(std::move(method), std::move(params),
                                    std::move(createHandler));
    }

    bool
    JSONRPC::RunAsync(llarp_ev_loop* loop, const std::string& remote)
    {
      strncpy(m_connect.remote, remote.c_str(), sizeof(m_connect.remote));
      // TODO: ipv6
      m_connect.connected = &JSONRPC::OnConnected;
      m_connect.error     = &JSONRPC::OnConnectFail;
      m_connect.user      = this;
      m_connect.af        = AF_INET;
      m_Loop              = loop;
      return true;
    }

    void
    JSONRPC::OnConnectFail(llarp_tcp_connecter* tcp)
    {
      JSONRPC* self = static_cast< JSONRPC* >(tcp->user);
      llarp::LogError("failed to connect to RPC, dropped all pending calls");
      self->DropAllCalls();
    }

    void
    JSONRPC::OnConnected(llarp_tcp_connecter* tcp, llarp_tcp_conn* conn)
    {
      JSONRPC* self = static_cast< JSONRPC* >(tcp->user);
      llarp::LogDebug("connected to RPC");
      self->Connected(conn);
    }

    void
    JSONRPC::Connected(llarp_tcp_conn* conn)
    {
      if(!m_Run)
      {
        llarp_tcp_conn_close(conn);
        return;
      }
      auto& front = m_PendingCalls.front();
      ConnImpl* connimpl =
          new ConnImpl(conn, std::move(front.method), std::move(front.params),
                       std::move(front.createHandler));
      m_PendingCalls.pop_front();
      m_Conns.emplace_back(connimpl->handler);
      connimpl->SendRequest();
    }

    void
    JSONRPC::Stop()
    {
      m_Run.store(false);
      DropAllCalls();
    }

    void
    JSONRPC::DropAllCalls()
    {
      while(m_PendingCalls.size())
      {
        auto& front          = m_PendingCalls.front();
        IRPCClientHandler* h = front.createHandler(nullptr);
        h->HandleError();
        delete h;
        m_PendingCalls.pop_front();
      }
    }

  }  // namespace http
}  // namespace abyss

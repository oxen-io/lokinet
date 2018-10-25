#include <abyss/server.hpp>
#include <llarp/time.h>
#include <sstream>
#include <unordered_map>
#include <string>
#include <llarp/logger.hpp>
#include <algorithm>

namespace abyss
{
  namespace http
  {
#if __cplusplus >= 201703L
    typedef std::string_view string_view;
#else
    typedef std::string string_view;
#endif
    struct RequestHeader
    {
      typedef std::unordered_multimap< std::string, std::string > Headers_t;
      Headers_t Headers;
      std::string Method;
      std::string Path;
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
      std::unique_ptr< abyss::json::IParser > m_BodyParser;
      json::Document m_Request;
      json::Document m_Response;

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
        // set up tcp members
        _conn->user   = this;
        _conn->read   = &ConnImpl::OnRead;
        _conn->tick   = &ConnImpl::OnTick;
        _conn->closed = &ConnImpl::OnClosed;
        m_Bad         = false;
        m_State       = eReadHTTPMethodLine;
      }

      ~ConnImpl()
      {
      }

      bool
      FeedLine(std::string& line)
      {
        switch(m_State)
        {
          case eReadHTTPMethodLine:
            return ProcessMethodLine(line);
          case eReadHTTPHeaders:
            return ProcessHeaderLine(line);
          default:
            return false;
        }
      }

      bool
      ProcessMethodLine(string_view line)
      {
        auto idx = line.find_first_of(' ');
        if(idx == string_view::npos)
          return false;
        m_Header.Method = line.substr(0, idx);
        line            = line.substr(idx + 1);
        idx             = line.find_first_of(' ');
        if(idx == string_view::npos)
          return false;
        m_Header.Path = line.substr(0, idx);
        m_State       = eReadHTTPHeaders;
        return true;
      }

      bool
      ShouldProcessHeader(const string_view& name) const
      {
        // TODO: header whitelist
        return name == "content-type" || name == "content-length";
      }

      bool
      ProcessHeaderLine(string_view line)
      {
        if(line.size() == 0)
        {
          // end of headers
          m_State = eReadHTTPBody;
          return true;
        }
        auto idx = line.find_first_of(':');
        if(idx == string_view::npos)
          return false;
        string_view header = line.substr(0, idx);
        string_view val    = line.substr(1 + idx);
        // to lowercase
        std::transform(header.begin(), header.end(), header.begin(),
                       [](char ch) -> char { return ::tolower(ch); });
        if(ShouldProcessHeader(header))
        {
          val = val.substr(val.find_first_not_of(' '));
          m_Header.Headers.insert(std::make_pair(header, val));
        }
        return true;
      }

      bool
      WriteStatusLine(int code, const std::string& message)
      {
        char buf[128] = {0};
        int sz        = snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s\r\n", code,
                          message.c_str());
        if(sz > 0)
        {
          return llarp_tcp_conn_async_write(_conn, buf, sz);
        }
        else
          return false;
      }

      bool
      WriteResponseSimple(int code, const std::string& msg,
                          const char* contentType, const char* content)
      {
        if(!WriteStatusLine(code, msg))
          return false;
        char buf[128] = {0};
        int sz =
            snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", contentType);
        if(sz <= 0)
          return false;
        if(!llarp_tcp_conn_async_write(_conn, buf, sz))
          return false;
        size_t contentLength = strlen(content);
        sz = snprintf(buf, sizeof(buf), "Content-Length: %zu\r\n\r\n",
                      contentLength);
        if(sz <= 0)
          return false;
        if(!llarp_tcp_conn_async_write(_conn, buf, sz))
          return false;
        if(!llarp_tcp_conn_async_write(_conn, content, contentLength))
          return false;
        m_State = eWriteHTTPBody;
        return true;
      }

      bool
      FeedBody(const char* buf, size_t sz)
      {
        if(m_Header.Method != "POST")
        {
          return WriteResponseSimple(405, "Method Not Allowed", "text/plain",
                                     "nope");
        }
        {
          auto itr = m_Header.Headers.find("content-type");
          if(itr == m_Header.Headers.end())
          {
            return WriteResponseSimple(415, "Unsupported Media Type",
                                       "text/plain",
                                       "no content type provided");
          }
          else if(itr->second != "application/json")
          {
            return WriteResponseSimple(415, "Unsupported Media Type",
                                       "text/plain",
                                       "this does not look like jsonrpc 2.0");
          }
        }
        // initialize body parser
        if(m_BodyParser == nullptr)
        {
          ssize_t contentLength = 0;
          auto itr              = m_Header.Headers.find("content-length");
          if(itr == m_Header.Headers.end())
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain",
                                       "no content length");
          }
          contentLength = std::stoll(itr->second);
          if(contentLength <= 0)
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain",
                                       "bad content length");
          }
          else
          {
            m_BodyParser.reset(abyss::json::MakeParser(contentLength));
          }
        }
        if(!m_BodyParser->FeedData(buf, sz))
        {
          return WriteResponseSimple(400, "Bad Request", "text/plain",
                                     "invalid body size");
        }
        switch(m_BodyParser->Parse(m_Request))
        {
          case json::IParser::eNeedData:
            return true;
          case json::IParser::eParseError:
            return WriteResponseSimple(400, "Bad Request", "text/plain",
                                       "bad json object");
          case json::IParser::eDone:
            if(m_Request.IsObject() && m_Request.HasMember("params")
               && m_Request.HasMember("method") && m_Request.HasMember("id")
               && (m_Request["id"].IsString() || m_Request["id"].IsNumber())
               && m_Request["method"].IsString()
               && m_Request["params"].IsObject())
            {
              m_Response.SetObject();
              m_Response.AddMember("jsonrpc",
                                   abyss::json::Value().SetString("2.0"),
                                   m_Response.GetAllocator());
              m_Response.AddMember("id", m_Request["id"],
                                   m_Response.GetAllocator());
              if(handler->HandleJSONRPC(m_Request["method"].GetString(),
                                        m_Request["params"].GetObject(),
                                        m_Response))
              {
                return WriteResponseJSON();
              }
            }
            return WriteResponseSimple(500, "internal error", "text/plain",
                                       "nope");
          default:
            return false;
        }
      }

      bool
      WriteResponseJSON()
      {
        std::string response;
        json::ToString(m_Response, response);
        return WriteResponseSimple(200, "OK", "application/json",
                                   response.c_str());
      }

      bool
      ProcessRead(const char* buf, size_t sz)
      {
        if(m_Bad)
        {
          return false;
        }

        m_LastActive = llarp_time_now_ms();
        if(m_State < eReadHTTPBody)
        {
          const char* end = strstr(buf, "\r\n");
          while(end)
          {
            string_view line(buf, end);
            switch(m_State)
            {
              case eReadHTTPMethodLine:
                if(!ProcessMethodLine(line))
                  return false;
                sz -= line.size() + (2 * sizeof(char));
                break;
              case eReadHTTPHeaders:
                if(!ProcessHeaderLine(line))
                  return false;
                sz -= line.size() + (2 * sizeof(char));
                break;
              default:
                break;
            }
            buf = end + (2 * sizeof(char));
            end = strstr(buf, "\r\n");
          }
        }
        if(m_State == eReadHTTPBody)
          return FeedBody(buf, sz);
        return false;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const void* buf, size_t sz)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        if(!self->ProcessRead((const char*)buf, sz))
          self->MarkBad();
      }

      static void
      OnClosed(llarp_tcp_conn* conn)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        self->_conn    = nullptr;
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
        return now - m_LastActive > m_ReadTimeout || m_Bad
            || m_State == eCloseMe;
      }

      void
      Close()
      {
        if(_conn)
        {
          llarp_tcp_conn_close(_conn);
          _conn = nullptr;
        }
      }
    };  // namespace http

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
      m_acceptor.tick     = &OnTick;
      m_acceptor.closed   = nullptr;
    }

    bool
    BaseReqHandler::ServeAsync(llarp_ev_loop* loop, llarp_logic* logic,
                               const sockaddr* bindaddr)
    {
      m_loop  = loop;
      m_Logic = logic;
      return llarp_tcp_serve(m_loop, &m_acceptor, bindaddr);
    }

    void
    BaseReqHandler::OnTick(llarp_tcp_acceptor* tcp)
    {
      BaseReqHandler* self = static_cast< BaseReqHandler* >(tcp->user);

      self->Tick();
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
      self->m_Conns.emplace_back(rpcHandler);
    }
  }  // namespace http
}  // namespace abyss

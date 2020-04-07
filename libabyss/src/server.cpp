#include <abyss/server.hpp>

#include <abyss/http.hpp>
#include <util/buffer.hpp>
#include <util/logging/logger.hpp>
#include <util/time.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>

namespace abyss
{
  namespace httpd
  {
    namespace json = llarp::json;
    struct ConnImpl : abyss::http::HeaderReader
    {
      llarp_tcp_conn* _conn;
      IRPCHandler* handler;
      BaseReqHandler* _parent;
      llarp_time_t m_LastActive;
      llarp_time_t m_ReadTimeout;
      bool m_Bad;
      std::unique_ptr<json::IParser> m_BodyParser;
      nlohmann::json m_Request;

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
        handler = nullptr;
        m_LastActive = p->now();
        m_ReadTimeout = readtimeout;
        // set up tcp members
        _conn->user = this;
        _conn->read = &ConnImpl::OnRead;
        _conn->tick = &ConnImpl::OnTick;
        _conn->closed = &ConnImpl::OnClosed;
        m_Bad = false;
        m_State = eReadHTTPMethodLine;
      }

      ~ConnImpl() = default;

      bool
      FeedLine(std::string& line)
      {
        bool done = false;
        switch (m_State)
        {
          case eReadHTTPMethodLine:
            return ProcessMethodLine(line);
          case eReadHTTPHeaders:
            if (!ProcessHeaderLine(line, done))
              return false;
            if (done)
              m_State = eReadHTTPBody;
            return true;
          default:
            return false;
        }
      }

      bool
      ProcessMethodLine(string_view line)
      {
        auto idx = line.find_first_of(' ');
        if (idx == string_view::npos)
          return false;
        Header.Method = std::string(line.substr(0, idx));
        line = line.substr(idx + 1);
        idx = line.find_first_of(' ');
        if (idx == string_view::npos)
          return false;
        Header.Path = std::string(line.substr(0, idx));
        m_State = eReadHTTPHeaders;
        return true;
      }

      bool
      ShouldProcessHeader(const string_view& name) const
      {
        // TODO: header whitelist
        return name == string_view("content-type") || name == string_view("content-length")
            || name == string_view("host");
      }

      bool
      WriteResponseSimple(
          int code, const std::string& msg, const char* contentType, const char* content)
      {
        char buf[512] = {0};
        size_t contentLength = strlen(content);
        int sz = snprintf(
            buf,
            sizeof(buf),
            "HTTP/1.0 %d %s\r\nContent-Type: "
            "%s\r\nContent-Length: %zu\r\n\r\n",
            code,
            msg.c_str(),
            contentType,
            contentLength);
        if (sz <= 0)
          return false;
        if (!llarp_tcp_conn_async_write(_conn, llarp_buffer_t(buf, sz)))
          return false;

        m_State = eWriteHTTPBody;

        return llarp_tcp_conn_async_write(_conn, llarp_buffer_t(content, contentLength));
      }

      bool
      FeedBody(const char* buf, size_t sz)
      {
        if (Header.Method != "POST")
        {
          return WriteResponseSimple(405, "Method Not Allowed", "text/plain", "nope");
        }
        {
          auto itr = Header.Headers.find("content-type");
          if (itr == Header.Headers.end())
          {
            return WriteResponseSimple(
                415, "Unsupported Media Type", "text/plain", "no content type provided");
          }
          else if (itr->second != string_view("application/json"))
          {
            return WriteResponseSimple(
                415, "Unsupported Media Type", "text/plain", "this does not look like jsonrpc 2.0");
          }
        }
        // initialize body parser
        if (m_BodyParser == nullptr)
        {
          auto itr = Header.Headers.find("content-length");
          if (itr == Header.Headers.end())
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain", "no content length");
          }
          ssize_t contentLength = std::stoll(itr->second);
          if (contentLength <= 0)
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain", "bad content length");
          }
          else
          {
            m_BodyParser.reset(json::MakeParser(contentLength));
          }
          itr = Header.Headers.find("host");
          if (itr == Header.Headers.end())
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain", "no host header provided");
          }
          if (not handler->ValidateHost(itr->second))
          {
            return WriteResponseSimple(400, "Bad Request", "text/plain", "invalid host header");
          }
        }
        if (!m_BodyParser->FeedData(buf, sz))
        {
          return WriteResponseSimple(400, "Bad Request", "text/plain", "invalid body size");
        }

        switch (m_BodyParser->Parse(m_Request))
        {
          case json::IParser::eNeedData:
            return true;
          case json::IParser::eParseError:
            return WriteResponseSimple(400, "Bad Request", "text/plain", "bad json object");
          case json::IParser::eDone:
            if (m_Request.is_object() && m_Request.count("params") && m_Request.count("method")
                && m_Request.count("id") && m_Request["id"].is_string()
                && m_Request["method"].is_string() && m_Request["params"].is_object())
            {
              nlohmann::json response;
              response["jsonrpc"] = "2.0";
              response["id"] = m_Request["id"].get<std::string>();
              auto value = handler->HandleJSONRPC(
                  m_Request["method"].get<std::string>(), m_Request["params"]);

              if (!value.is_null())
                response["result"] = std::move(value);

              return WriteResponseJSON(response);
            }
            return WriteResponseSimple(500, "internal error", "text/plain", "nope");
          default:
            return false;
        }
      }

      bool
      WriteResponseJSON(const nlohmann::json& response)
      {
        std::string responseStr = response.dump();
        return WriteResponseSimple(200, "OK", "application/json", responseStr.c_str());
      }

      bool
      ProcessRead(const char* buf, size_t sz)
      {
        llarp::LogDebug("http read ", sz, " bytes");
        if (m_Bad)
        {
          return false;
        }

        if (!sz)
          return true;

        bool done = false;
        m_LastActive = _parent->now();

        if (m_State < eReadHTTPBody)
        {
          const char* end = strstr(buf, "\r\n");
          while (end)
          {
            string_view line(buf, end - buf);
            switch (m_State)
            {
              case eReadHTTPMethodLine:
                if (!ProcessMethodLine(line))
                  return false;
                sz -= line.size() + (2 * sizeof(char));
                break;
              case eReadHTTPHeaders:
                if (!ProcessHeaderLine(line, done))
                  return false;
                sz -= line.size() + (2 * sizeof(char));
                if (done)
                  m_State = eReadHTTPBody;
                break;
              default:
                break;
            }
            buf = end + (2 * sizeof(char));
            end = strstr(buf, "\r\n");
          }
        }
        if (m_State == eReadHTTPBody)
          return FeedBody(buf, sz);
        return false;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const llarp_buffer_t& buf)
      {
        ConnImpl* self = static_cast<ConnImpl*>(conn->user);
        if (!self->ProcessRead((const char*)buf.base, buf.sz))
          self->MarkBad();
      }

      static void
      OnClosed(llarp_tcp_conn* conn)
      {
        llarp::LogDebug("connection closed");
        ConnImpl* self = static_cast<ConnImpl*>(conn->user);
        self->_conn = nullptr;
        self->m_State = eCloseMe;
      }

      static void
      OnTick(llarp_tcp_conn* conn)
      {
        ConnImpl* self = static_cast<ConnImpl*>(conn->user);
        self->Tick();
      }

      void
      Tick()
      {
        if (m_Bad)
          Close();
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
        return now - m_LastActive > m_ReadTimeout || m_Bad || m_State == eCloseMe;
      }

      void
      Close()
      {
        if (_conn)
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

    BaseReqHandler::BaseReqHandler(llarp_time_t reqtimeout) : m_ReqTimeout(reqtimeout)
    {
      m_acceptor.accepted = &BaseReqHandler::OnAccept;
      m_acceptor.user = this;
      m_acceptor.tick = &OnTick;
      m_acceptor.closed = nullptr;
    }

    bool
    BaseReqHandler::ServeAsync(
        llarp_ev_loop_ptr loop, std::shared_ptr<llarp::Logic> logic, const sockaddr* bindaddr)
    {
      m_loop = loop;
      m_Logic = logic;
      return llarp_tcp_serve(m_loop.get(), &m_acceptor, bindaddr);
    }

    void
    BaseReqHandler::OnTick(llarp_tcp_acceptor* tcp)
    {
      BaseReqHandler* self = static_cast<BaseReqHandler*>(tcp->user);

      self->Tick();
    }

    void
    BaseReqHandler::Tick()
    {
      auto _now = now();
      auto itr = m_Conns.begin();
      while (itr != m_Conns.end())
      {
        if ((*itr)->ShouldClose(_now))
          itr = m_Conns.erase(itr);
        else
          ++itr;
      }
    }

    void
    BaseReqHandler::Close()
    {
      llarp_tcp_acceptor_close(&m_acceptor);
    }

    BaseReqHandler::~BaseReqHandler()
    {
    }

    void
    BaseReqHandler::OnAccept(llarp_tcp_acceptor* acceptor, llarp_tcp_conn* conn)
    {
      BaseReqHandler* self = static_cast<BaseReqHandler*>(acceptor->user);
      ConnImpl* connimpl = new ConnImpl(self, conn, self->m_ReqTimeout);
      IRPCHandler* rpcHandler = self->CreateHandler(connimpl);
      if (rpcHandler == nullptr)
      {
        connimpl->Close();
        delete connimpl;
        return;
      }
      connimpl->handler = rpcHandler;
      self->m_Conns.emplace_back(rpcHandler);
    }
  }  // namespace httpd
}  // namespace abyss

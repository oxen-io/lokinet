#include <abyss/client.hpp>
#include <abyss/http.hpp>
#include <abyss/md5.hpp>
#include <crypto/crypto.hpp>
#include <util/buffer.hpp>
#include <util/logging/logger.hpp>

namespace abyss
{
  namespace http
  {
    namespace json = llarp::json;
    struct ConnImpl : HeaderReader
    {
      llarp_tcp_conn* m_Conn;
      JSONRPC* m_Parent;
      nlohmann::json m_RequestBody;
      Headers_t m_SendHeaders;
      IRPCClientHandler* handler;
      std::unique_ptr< json::IParser > m_BodyParser;
      nlohmann::json m_Response;
      uint16_t m_AuthTries;
      bool m_ShouldAuth;
      enum State
      {
        eInitial,
        eReadStatusLine,
        eReadResponseHeader,
        eReadResponseBody,
        eCloseMe
      };

      State state;

      ConnImpl(llarp_tcp_conn* conn, JSONRPC* parent,
               const RPC_Method_t& method, const RPC_Params& params,
               JSONRPC::HandlerFactory factory)
          : m_Conn(conn)
          , m_Parent(parent)
          , m_RequestBody(nlohmann::json::object())
          , m_Response(nlohmann::json::object())
          , m_AuthTries(0)
          , m_ShouldAuth(false)
          , state(eInitial)
      {
        conn->user   = this;
        conn->closed = &ConnImpl::OnClosed;
        conn->read   = &ConnImpl::OnRead;
        conn->tick   = &ConnImpl::OnTick;

        handler = factory(this);

        m_RequestBody["jsonrpc"] = "2.0";
        llarp::AlignedBuffer< 8 > p;
        p.Randomize();
        m_RequestBody["id"]     = p.ToHex();
        m_RequestBody["method"] = method;
        m_RequestBody["params"] = params;
      }

      static void
      OnClosed(llarp_tcp_conn* conn)
      {
        llarp::LogDebug("connection closed");
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        self->state    = eCloseMe;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const llarp_buffer_t& buf)
      {
        ConnImpl* self = static_cast< ConnImpl* >(conn->user);
        if(!self->ProcessRead((const char*)buf.base, buf.sz))
        {
          self->CloseError("on read failed");
        }
      }

      static void
      OnTick(llarp_tcp_conn* /*conn*/)
      {
      }

      bool
      ProcessStatusLine(string_view line)
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
      ShouldProcessHeader(const llarp::string_view& name) const
      {
        return name == llarp::string_view("content-length")
            || name == llarp::string_view("content-type")
            || name == llarp::string_view("www-authenticate");
      }

      /// return true if we get a 200 status code
      bool
      HandleStatusCode(string_view code)
      {
        if(code == string_view("200"))
          return true;
        if(code == string_view("401"))
        {
          m_ShouldAuth = true;
          return true;
        }
        return false;
      }

      bool
      RetryWithAuth(const std::string& auth)
      {
        m_ShouldAuth = false;
        auto idx     = auth.find_first_of(' ');
        if(idx == std::string::npos)
          return false;
        std::istringstream info(auth.substr(1 + idx));
        std::unordered_map< std::string, std::string > opts;
        std::string part;
        while(std::getline(info, part, ','))
        {
          idx = part.find_first_of('=');
          if(idx == std::string::npos)
            return false;
          std::string k = part.substr(0, idx);
          std::string val;
          ++idx;
          while(idx < part.size())
          {
            const char ch = part.at(idx);
            val += ch;
            ++idx;
          }
          opts[k] = val;
        }
        auto itr = opts.find("algorithm");

        if(itr != opts.end() && itr->second == "MD5-sess")
          return false;

        std::stringstream authgen;

        auto strip = [&opts](const std::string& name) -> std::string {
          std::string val;
          std::for_each(opts[name].begin(), opts[name].end(),
                        [&val](const char& ch) {
                          if(ch != '"')
                            val += ch;
                        });
          return val;
        };

        const auto realm       = strip("realm");
        const auto nonce       = strip("nonce");
        const auto qop         = strip("qop");
        std::string nonceCount = "0000000" + std::to_string(m_AuthTries);

        std::string str =
            m_Parent->username + ":" + realm + ":" + m_Parent->password;
        std::string h1 = MD5::SumHex(str);
        str            = "POST:/json_rpc";
        std::string h2 = MD5::SumHex(str);
        llarp::AlignedBuffer< 8 > n;
        n.Randomize();
        std::string cnonce = n.ToHex();
        str = h1 + ":" + nonce + ":" + nonceCount + ":" + cnonce + ":" + qop
            + ":" + h2;

        auto responseH = MD5::SumHex(str);
        authgen << "Digest username=\"" << m_Parent->username + "\", realm=\""
                << realm
                << "\", uri=\"/json_rpc\", algorithm=MD5, qop=auth, nonce=\""
                << nonce << "\", response=\"" << responseH
                << "\", nc=" << nonceCount << ", cnonce=\"" << cnonce << "\"";
        for(const auto& opt : opts)
        {
          if(opt.first == "algorithm" || opt.first == "realm"
             || opt.first == "qop" || opt.first == "nonce"
             || opt.first == "stale")
            continue;
          authgen << ", " << opt.first << "=" << opt.second;
        }
        m_SendHeaders.clear();
        m_SendHeaders.emplace("Authorization", authgen.str());
        SendRequest();
        return true;
      }

      bool
      ProcessBody(const char* buf, size_t sz)
      {
        // we got 401 ?
        if(m_ShouldAuth && m_AuthTries < 9)
        {
          m_AuthTries++;
          auto range = Header.Headers.equal_range("www-authenticate");
          auto itr   = range.first;
          while(itr != range.second)
          {
            if(RetryWithAuth(itr->second))
              return true;
            else
              ++itr;
          }
          return false;
        }
        // init parser
        if(m_BodyParser == nullptr)
        {
          size_t contentSize = 0;
          auto itr           = Header.Headers.find("content-length");
          // no content-length header
          if(itr == Header.Headers.end())
            return false;
          contentSize = std::stoul(itr->second);
          m_BodyParser.reset(json::MakeParser(contentSize));
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
              CloseError("json parse error");
              return true;
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
      CloseError(const char* msg)
      {
        LogError("CloseError: ", msg);
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
        std::stringstream ss;
        body = m_RequestBody.dump();
        m_SendHeaders.emplace("Content-Type", "application/json");
        m_SendHeaders.emplace("Content-Length", std::to_string(body.size()));
        m_SendHeaders.emplace("Accept", "application/json");
        std::stringstream request;
        request << "POST /json_rpc HTTP/1.1\r\n";
        for(const auto& item : m_SendHeaders)
          request << item.first << ": " << item.second << "\r\n";
        request << "\r\n" << body;
        std::string buf = request.str();

        if(!llarp_tcp_conn_async_write(m_Conn,
                                       llarp_buffer_t(buf.c_str(), buf.size())))
        {
          CloseError("failed to write request");
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
        llarp_tcp_async_try_connect(m_Loop.get(), &m_connect);
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
    JSONRPC::RunAsync(llarp_ev_loop_ptr loop, const std::string& remote)
    {
      strncpy(m_connect.remote, remote.c_str(), sizeof(m_connect.remote) - 1);
      // TODO: ipv6
      m_connect.connected = &JSONRPC::OnConnected;
      m_connect.error     = &JSONRPC::OnConnectFail;
      m_connect.user      = this;
      m_connect.af        = AF_INET;
      m_Loop              = std::move(loop);
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
      auto& front        = m_PendingCalls.front();
      ConnImpl* connimpl = new ConnImpl(conn, this, front.method, front.params,
                                        front.createHandler);
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

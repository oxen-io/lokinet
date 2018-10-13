#include <llarp/arpc.hpp>

namespace llarp
{
  namespace arpc
  {
    /// interface for request messages
    struct IRequest
    {
      /// returns false if errmsg is set
      /// returns true if retval is set
      virtual bool
      HandleRequest(Server* ctx, std::unique_ptr< BaseMessage >& retval,
                    std::string& errmsg) const = 0;
    };

    struct BaseMessage : public llarp::IBEncodeMessage, public IRequest
    {
      static constexpr size_t MaxIDSize = 128;
      /// maximum size of a message
      static constexpr size_t MaxSize = 1024 * 8;

      BaseMessage()
      {
        timestamp = llarp_time_now_ms();
        zkey.Zero();
        zsig.Zero();
      }

      std::string m_id;
      llarp_time_t timestamp;
      llarp::PubKey zkey;
      llarp::Signature zsig;

      /// override me
      virtual std::string
      Method() const = 0;

      /// encode the entire message
      bool
      BEncode(llarp_buffer_t* buf) const
      {
        if(!bencode_start_dict(buf))
          return false;
        if(!BEncodeWriteDictString("aRPC-method", Method(), buf))
          return false;
        if(!BEncodeWriteDictString("id", m_id, buf))
          return false;
        if(!BEncodeBody(buf))
          return false;
        if(!zkey.IsZero())
        {
          if(!BEncodeWriteDictEntry("z-key", zkey, buf))
            return false;
          if(!BEncodeWriteDictEntry("z-sig", zsig, buf))
            return false;
        }
        return bencode_end(buf);
      }

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
      {
        if(llarp_buffer_eq(k, "id"))
        {
          return DecodeID(buf);
        }
        if(llarp_buffer_eq(k, "params"))
        {
          return DecodeParams(buf);
        }
        return false;
      }

     protected:
      typedef bool (*ParamDecoder)(dict_reader*, llarp_buffer_t*);

      virtual ParamDecoder
      GetParamDecoder() const = 0;

      bool
      DecodeParams(llarp_buffer_t* buf)
      {
        dict_reader r;
        r.user   = this;
        r.on_key = GetParamDecoder();
        return bencode_read_dict(buf, &r);
      }

      bool
      DecodeID(llarp_buffer_t* buf)
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(buf, &strbuf))
          return false;
        if(strbuf.sz > MaxIDSize)  // too big
          return false;
        m_id = std::string((char*)strbuf.base, strbuf.sz);
        return true;
      }

      /// encode body of message
      virtual bool
      BEncodeBody(llarp_buffer_t* buf) const = 0;
    };

    struct ConnHandler
    {
      ConnHandler(Server* s, llarp_tcp_conn* c) : parent(s), m_conn(c)
      {
        left          = 0;
        readingHeader = true;
      }

      bool readingHeader;
      Server* parent;
      llarp_tcp_conn* m_conn;
      AlignedBuffer< BaseMessage::MaxSize > buf;
      uint16_t left;

      void
      ParseMessage();

      void
      Close()
      {
        llarp_tcp_conn_close(m_conn);
      }

      static void
      OnClosed(llarp_tcp_conn* conn)
      {
        ConnHandler* self = static_cast< ConnHandler* >(conn->user);
        delete self;
      }

      static void
      OnRead(llarp_tcp_conn* conn, const void* buf, size_t sz)
      {
        ConnHandler* self = static_cast< ConnHandler* >(conn->user);
        const byte_t* ptr = (const byte_t*)buf;
        do
        {
          if(self->readingHeader)
          {
            self->left = bufbe16toh(ptr);
            sz -= 2;
            ptr += 2;
            self->readingHeader = false;
          }
          size_t dlt = std::min((size_t)self->left, sz);
          memcpy(self->buf.data() + (self->buf.size() - self->left), ptr, dlt);
          self->left -= dlt;
          sz -= dlt;
          if(self->left == 0)
          {
            self->ParseMessage();
            self->readingHeader = true;
          }
        } while(sz > 0);
      }
    };

    /// base type for ping req/resp
    struct Ping : public BaseMessage
    {
      Ping() : BaseMessage()
      {
      }

      uint64_t ping;

      std::string
      Method() const
      {
        return "llarp.rpc.ping";
      }

      bool
      BEncodeBody(llarp_buffer_t* buf) const
      {
        if(!bencode_write_bytestring(buf, "params", 6))
          return false;
        if(!bencode_start_dict(buf))
          return false;

        if(!BEncodeWriteDictInt("ping", ping, buf))
          return false;
        return bencode_end(buf);
      }

      static bool
      OnParamKey(dict_reader* r, llarp_buffer_t* k)
      {
        Ping* self = static_cast< Ping* >(r->user);
        if(k && llarp_buffer_eq(*k, "ping"))
        {
          return bencode_read_integer(r->buffer, &self->ping);
        }
        else
          return k == nullptr;
      }

      virtual ParamDecoder
      GetParamDecoder() const
      {
        return &OnParamKey;
      }
    };

    struct PingResponse : public Ping
    {
      PingResponse(uint64_t p) : Ping()
      {
        ping = p;
      }

      bool
      HandleRequest(Server*, std::unique_ptr< BaseMessage >&,
                    std::string&) const
      {
        /// TODO: handle client response
        llarp::LogInfo(Method(), "pong ", ping);
        return false;
      }
    };

    struct PingRequest : public Ping
    {
      bool
      HandleRequest(Server* serv, std::unique_ptr< BaseMessage >& retval,
                    std::string& errmsg) const
      {
        PingResponse* resp = new PingResponse(ping);
        if(!serv->Sign(resp))
        {
          errmsg = "failed to sign response";
          return false;
        }
        retval.reset(resp);
        return true;
      }
    };

    struct MessageReader
    {
      dict_reader m_reader;
      BaseMessage* msg = nullptr;

      MessageReader()
      {
        m_reader.user   = this;
        m_reader.on_key = &OnKey;
      }

      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key)
      {
        static std::unordered_map< std::string,
                                   const std::function< BaseMessage*(void) > >
            msgConstructors = {
                {"llarp.rpc.ping",
                 []() -> BaseMessage* { return new PingRequest(); }},
            };

        MessageReader* self = static_cast< MessageReader* >(r->user);
        if(self->msg == nullptr)
        {
          // first key
          if(key == nullptr || !llarp_buffer_eq(*key, "aRPC-method"))
          {
            // bad value
            return false;
          }
          llarp_buffer_t strbuf;
          if(!bencode_read_string(r->buffer, &strbuf))
            return false;
          std::string method = std::string((char*)strbuf.base, strbuf.sz);
          auto itr           = msgConstructors.find(method);
          if(itr == msgConstructors.end())
          {
            // no such method
            return false;
          }
          else
            self->msg = itr->second();
          return true;
        }
        else if(key)
          return self->msg->DecodeKey(*key, r->buffer);
        else
          return true;
      }

      bool
      DecodeMessage(llarp_buffer_t* buf,
                    std::unique_ptr< BaseMessage >& request)
      {
        msg = nullptr;
        if(!bencode_read_dict(buf, &m_reader))
          return false;
        request.reset(msg);
        return true;
      }
    };

    Server::Server(llarp_router* r)
    {
      router              = r;
      m_acceptor.user     = this;
      m_acceptor.accepted = &OnAccept;
    }

    bool
    Server::Start(const std::string& bindaddr)
    {
      llarp::Addr addr;
      sockaddr* saddr = nullptr;
#ifndef _WIN32
      sockaddr_un unaddr;
      if(bindaddr.find("unix:") == 0)
      {
        unaddr.sun_family = AF_UNIX;

        strncpy(unaddr.sun_path, bindaddr.substr(5).c_str(),
                sizeof(unaddr.sun_path));
        saddr = (sockaddr*)&unaddr;
      }
      else
#endif
      {
        // TODO: ipv6
        auto idx         = bindaddr.find(':');
        std::string host = bindaddr.substr(0, idx);
        uint16_t port    = std::stoi(bindaddr.substr(idx + 1));
        addr             = llarp::Addr(host, port);
        saddr            = (sockaddr*)addr;
      }
      return llarp_tcp_serve(&m_acceptor, saddr);
    }

    void
    Server::OnAccept(llarp_tcp_acceptor* a, llarp_tcp_conn* conn)
    {
      Server* self = static_cast< Server* >(a->user);
      conn->user   = new ConnHandler(self, conn);
      conn->read   = &ConnHandler::OnRead;
      conn->closed = &ConnHandler::OnClosed;
    }

    bool
    Server::Sign(BaseMessage* msg) const
    {
      msg->zkey = SigningPublicKey();
      msg->zsig.Zero();
      llarp::Signature sig;
      //
      byte_t tmp[BaseMessage::MaxSize];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);

      if(!msg->BEncode(&buf))
        return false;
      // rewind buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;

      if(!Crypto()->sign(sig, SigningPrivateKey(), buf))
        return false;

      msg->zsig = sig;
      return true;
    }

    void
    ConnHandler::ParseMessage()
    {
      std::unique_ptr< BaseMessage > msg;
      std::unique_ptr< BaseMessage > response;
      std::string errmsg;
      MessageReader r;
      auto tmp = llarp::Buffer(buf);
      if(!r.DecodeMessage(&tmp, msg))
      {
        llarp::LogError("failed to decode message");
        Close();
        return;
      }

      // handle request
      if(!msg->HandleRequest(parent, response, errmsg))
      {
        // TODO: send error reply
        llarp::LogError("failed to handle api message: ", errmsg);
        Close();
        return;
      }

      if(!parent->Sign(response.get()))
      {
        llarp::LogError("failed to sign response");
        Close();
      }
    }
  }  // namespace arpc
}  // namespace llarp

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <llarp/api/base.hpp>
#include <llarp/api/client.hpp>
#include <llarp/api/messages.hpp>
#include <llarp/api/parser.hpp>

namespace llarp
{
  namespace api
  {
    struct ClientPImpl
    {
      ClientPImpl(const std::string& sessionName, llarp_ev_loop* evloop)
          : m_Base(evloop, this), m_SessionName(sessionName)
      {
      }

      ~ClientPImpl()
      {
        llarp_ev_loop_free(&m_Base.loop);
      }

      bool
      StartSession(const std::string& addr, uint16_t port)
      {
        inet_pton(AF_INET, addr.c_str(), &m_Base.apiAddr.sin_addr.s_addr);
        m_Base.apiAddr.sin_family = AF_INET;
        m_Base.apiAddr.sin_port   = htons(port);
        SpawnMessage msg;
        msg.seqno       = 0;
        msg.SessionName = m_SessionName;
        return m_Base.SendMessage(&msg);
      }

      void
      HandleMessage(const IMessage* msg)
      {
      }

      Base< ClientPImpl > m_Base;
      std::string m_SessionName;
    };

    Client::Client(const std::string& name)
    {
      llarp_ev_loop* loop = nullptr;
      llarp_ev_loop_alloc(&loop);
      m_Impl = new ClientPImpl(name, loop);
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
      if(!m_Impl->m_Base.BindDefault())
        return false;
      llarp::LogDebug("Bound Socket");
      std::string addr    = url.substr(0, url.find(":"));
      std::string strport = url.substr(url.find(":") + 1);
      int port            = std::stoi(strport);
      if(port == -1)
      {
        llarp::LogError("bad port: ", strport);
        return false;
      }
      llarp::LogDebug("starting session with ", addr, ":", port);
      return m_Impl->StartSession(addr, port);
    }

    int
    Client::Mainloop()
    {
      return m_Impl->m_Base.Mainloop();
    }

  }  // namespace api
}  // namespace llarp
#ifndef LOKINET_LOKINET_HPP
#define LOKINET_LOKINET_HPP
#include <cstdint>
#include <llarp/bencode.hpp>
#include <llarp/router_id.hpp>
#include <memory>
#include <vector>

namespace lokinet
{
  struct API_PImpl;
  struct RPCMessage : public llarp::IBEncodeMessage
  {
  };

  /// a persisting anonymized session to a serive node
  struct Handle
  {
    llarp::RouterID remote;

    bool
    Send(const RPCMessage* msg);

    RPCMessage*
    Recv();

    typedef std::shared_ptr< Handle > Ptr;

   private:
    API_PImpl* m_Impl;
  };

  struct Client
  {
    void
    Run();
  };

  struct IMessageHandler
  {
    virtual RPCMessage*
    HandleMessage(const RPCMessage* inmsg) = 0;
  };

  struct Server_PImpl;
  struct Server
  {
    Server(const uint8_t* secretkey, IMessageHandler* h);
    ~Server();

    void
    Run();

   private:
    Server_PImpl* m_Impl;
  };

}  // namespace lokinet

#endif
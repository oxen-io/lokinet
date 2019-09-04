#include <dns/server.hpp>

#include <crypto/crypto.hpp>
#include <util/thread/logic.hpp>
#include <array>
#include <utility>

namespace llarp
{
  namespace dns
  {
    Proxy::Proxy(llarp_ev_loop_ptr serverLoop, Logic_ptr serverLogic,
                 llarp_ev_loop_ptr clientLoop, Logic_ptr clientLogic,
                 IQueryHandler* h)
        : m_ServerLoop(std::move(serverLoop))
        , m_ClientLoop(std::move(clientLoop))
        , m_ServerLogic(std::move(serverLogic))
        , m_ClientLogic(std::move(clientLogic))
        , m_QueryHandler(h)
    {
      m_Client.user     = this;
      m_Server.user     = this;
      m_Client.tick     = nullptr;
      m_Server.tick     = nullptr;
      m_Client.recvfrom = &HandleUDPRecv_client;
      m_Server.recvfrom = &HandleUDPRecv_server;
    }

    void
    Proxy::Stop()
    {
    }

    bool
    Proxy::Start(const llarp::Addr addr,
                 const std::vector< llarp::Addr >& resolvers)
    {
      m_Resolvers.clear();
      m_Resolvers = resolvers;
      const llarp::Addr any("0.0.0.0", 0);
      auto self = shared_from_this();
      m_ClientLogic->queue_func([=]() {
        llarp_ev_add_udp(self->m_ClientLoop.get(), &self->m_Client, any);
      });
      m_ServerLogic->queue_func([=]() {
        llarp_ev_add_udp(self->m_ServerLoop.get(), &self->m_Server, addr);
      });
      return true;
    }

    void
    Proxy::HandleUDPRecv_server(llarp_udp_io* u, const sockaddr* from,
                                ManagedBuffer buf)
    {
      static_cast< Proxy* >(u->user)->HandlePktServer(*from, &buf.underlying);
    }

    void
    Proxy::HandleUDPRecv_client(llarp_udp_io* u, const sockaddr* from,
                                ManagedBuffer buf)
    {
      static_cast< Proxy* >(u->user)->HandlePktClient(*from, &buf.underlying);
    }

    llarp::Addr
    Proxy::PickRandomResolver() const
    {
      const size_t sz = m_Resolvers.size();
      if(sz <= 1)
        return m_Resolvers[0];
      auto itr = m_Resolvers.begin();
      std::advance(itr, llarp::randint() % sz);
      return *itr;
    }

    void
    Proxy::HandleTick(llarp_udp_io*)
    {
    }

    void
    Proxy::SendServerMessageTo(llarp::Addr to, Message msg)
    {
      auto self = shared_from_this();
      m_ServerLogic->queue_func([to, msg, self]() {
        std::array< byte_t, 1500 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if(msg.Encode(&buf))
        {
          buf.sz  = buf.cur - buf.base;
          buf.cur = buf.base;
          llarp_ev_udp_sendto(&self->m_Server, to, buf);
        }
        else
          llarp::LogWarn("failed to encode dns message when sending");
      });
    }

    void
    Proxy::SendClientMessageTo(llarp::Addr to, Message msg)
    {
      auto self = shared_from_this();
      m_ClientLogic->queue_func([to, msg, self]() {
        std::array< byte_t, 1500 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if(msg.Encode(&buf))
        {
          buf.sz  = buf.cur - buf.base;
          buf.cur = buf.base;
          llarp_ev_udp_sendto(&self->m_Client, to, buf);
        }
        else
          llarp::LogWarn("failed to encode dns message when sending");
      });
    }

    void
    Proxy::HandlePktClient(llarp::Addr from, llarp_buffer_t* pkt)
    {
      MessageHeader hdr;
      if(!hdr.Decode(pkt))
      {
        llarp::LogWarn("failed to parse dns header from ", from);
        return;
      }
      TX tx    = {hdr.id, from};
      auto itr = m_Forwarded.find(tx);
      if(itr == m_Forwarded.end())
        return;

      const Addr requester = itr->second;
      std::vector< byte_t > tmp(pkt->sz);
      std::copy_n(pkt->base, pkt->sz, tmp.begin());
      auto self = shared_from_this();
      m_ServerLogic->queue_func([=]() {
        // forward reply to requester via server
        llarp_buffer_t tmpbuf(tmp);
        llarp_ev_udp_sendto(&self->m_Server, requester, tmpbuf);
      });
      // remove pending
      m_Forwarded.erase(itr);
    }

    void
    Proxy::HandlePktServer(llarp::Addr from, llarp_buffer_t* pkt)
    {
      MessageHeader hdr;
      if(!hdr.Decode(pkt))
      {
        llarp::LogWarn("failed to parse dns header from ", from);
        return;
      }
      TX tx    = {hdr.id, from};
      auto itr = m_Forwarded.find(tx);
      Message msg(hdr);
      if(!msg.Decode(pkt))
      {
        llarp::LogWarn("failed to parse dns message from ", from);
        return;
      }
      auto self = shared_from_this();
      if(m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
      {
        if(!m_QueryHandler->HandleHookedDNSMessage(
               std::move(msg),
               std::bind(&Proxy::SendServerMessageTo, self, from,
                         std::placeholders::_1)))
        {
          llarp::LogWarn("failed to handle hooked dns");
        }
      }
      else if(m_Resolvers.size() == 0)
      {
        // no upstream resolvers
        // let's serv fail it
        msg.AddServFail();

        SendServerMessageTo(from, std::move(msg));
      }
      else if(itr == m_Forwarded.end())
      {
        // new forwarded query
        tx.from         = PickRandomResolver();
        m_Forwarded[tx] = from;
        std::vector< byte_t > tmp(pkt->sz);
        std::copy_n(pkt->base, pkt->sz, tmp.begin());

        m_ClientLogic->queue_func([=] {
          // do query
          llarp_buffer_t buf(tmp);
          llarp_ev_udp_sendto(&self->m_Client, tx.from, buf);
        });
      }
      else
      {
        // drop (?)
      }
    }

  }  // namespace dns
}  // namespace llarp

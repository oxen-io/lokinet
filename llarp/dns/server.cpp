#include <dns/server.hpp>

#include <crypto/crypto.hpp>

#include <array>

namespace llarp
{
  namespace dns
  {
    Proxy::Proxy(llarp_ev_loop* loop, IQueryHandler* h)
        : m_Loop(loop), m_QueryHandler(h)
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
    Proxy::Start(const llarp::Addr& addr,
                 const std::vector< llarp::Addr >& resolvers)
    {
      m_Resolvers.clear();
      m_Resolvers = resolvers;
      if(m_Resolvers.size() == 0)
      {
        llarp::LogError("no upstream dns provide specified");
        return false;
      }
      llarp::Addr any("0.0.0.0", 0);
      return llarp_ev_add_udp(m_Loop, &m_Server, addr) == 0
          && llarp_ev_add_udp(m_Loop, &m_Client, any) == 0;
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
      size_t sz = m_Resolvers.size();
      if(sz == 0)
        return llarp::Addr("8.8.8.8", 53);
      if(sz == 1)
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
    Proxy::SendMessageTo(llarp::Addr to, Message msg)
    {
      std::array< byte_t, 1500 > tmp = {{0}};
      llarp_buffer_t buf(tmp);
      if(msg.Encode(&buf))
      {
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        llarp_ev_udp_sendto(&m_Server, to, buf);
      }
      else
        llarp::LogWarn("failed to encode dns message when sending");
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
      if(itr != m_Forwarded.end())
      {
        llarp_buffer_t buf;
        buf.sz   = pkt->sz;
        buf.base = pkt->base;
        buf.cur  = buf.base;
        // forward reply
        llarp_ev_udp_sendto(&m_Server, itr->second, buf);
        // remove pending
        m_Forwarded.erase(itr);
      }
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

      if(m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
      {
        if(!m_QueryHandler->HandleHookedDNSMessage(
               std::move(msg),
               std::bind(&Proxy::SendMessageTo, this, from,
                         std::placeholders::_1)))
        {
          llarp::LogWarn("failed to handle hooked dns");
        }
        return;
      }
      else if(itr == m_Forwarded.end())
      {
        // new forwarded query
        tx.from         = PickRandomResolver();
        m_Forwarded[tx] = from;
        llarp_buffer_t buf;
        buf.sz   = pkt->sz;
        buf.base = pkt->base;
        buf.cur  = buf.base;
        // do query
        llarp_ev_udp_sendto(&m_Client, tx.from, buf);
      }
      else
      {
        // drop (?)
      }
    }

  }  // namespace dns
}  // namespace llarp

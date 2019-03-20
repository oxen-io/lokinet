#include <dns/dns.hpp>
#include <dns/message.hpp>
#include <functional>
#include <util/logger.hpp>
#include <router_id.hpp>
#include <router/router.hpp>

namespace llarp
{

/*
  void
  EnsurePathToSNode(const RouterID& snode, SNodeEnsureHook h, SNodeSessions &m_SNodeSessions)
  {
    if(m_SNodeSessions.count(snode) == 0)
    {
      auto themIP = ObtainIPForAddr(snode, true);
      m_SNodeSessions.emplace(
          snode,
          std::make_unique< exit::SNodeSession >(
              snode,
              std::bind(&Endpoint::HandleWriteIPPacket, this,
                        std::placeholders::_1,
                        [themIP]() -> huint32_t { return themIP; }),
              m_Router, 2, numHops));
    }
    auto range = m_SNodeSessions.equal_range(snode);
    auto itr   = range.first;
    while(itr != range.second)
    {
      if(itr->second->IsReady())
        h(snode, itr->second.get());
      else
        itr->second->AddReadyHook(std::bind(h, snode, std::placeholders::_1));
      ++itr;
    }
  }
*/

  template < typename Addr_t, typename Endpoint_t >
  void
  SendDNSReply(Addr_t addr, Endpoint_t* ctx, dns::Message* query,
               std::function< void(dns::Message) > reply, bool snode)
  {
    if(ctx)
    {
      huint32_t ip = ObtainIPForAddr(addr, snode);
      query->AddINReply(ip);
    }
    else
      query->AddNXReply();
    reply(*query);
    delete query;
  }

  bool
  is_random_snode(const dns::Message &msg)
  {
    return msg.questions[0].qname == "random.snode"
        || msg.questions[0].qname == "random.snode.";
  }

  bool
  is_localhost_loki(const dns::Message &msg)
  {
    return msg.questions[0].qname == "localhost.loki"
        || msg.questions[0].qname == "localhost.loki.";
  }

// FIXME: we need support more than one local IPs
  bool
  llarp_HandleHookedDNSMessage(
      dns::Message &&msg, std::function< void(dns::Message) > reply, AbstractRouter *router, huint32_t local_ip)
  {
    llarp::LogInfo("DNS.HandleHookedDNSMessage ", msg.questions[0].qname, " of type ", msg.questions[0].qtype);
    llarp::LogInfo((is_random_snode(msg)?"random":""), is_localhost_loki(msg)?"localhost":"");
    if(msg.questions.size() != 1)
    {
      llarp::LogWarn("bad number of dns questions: ", msg.questions.size());
      return false;
    }
    std::string qname = msg.questions[0].qname;
    if(msg.questions[0].qtype == dns::qTypeMX)
    {
      // mx record
      service::Address addr;
      if(addr.FromString(qname, ".loki") || addr.FromString(qname, ".snode")
         || is_random_snode(msg) || is_localhost_loki(msg))
        msg.AddMXReply(qname, 1);
      else
        msg.AddNXReply();
      reply(msg);
    }
    else if(msg.questions[0].qtype == dns::qTypeCNAME)
    {
      if(is_random_snode(msg))
      {
        RouterID random;
        if(router->GetRandomGoodRouter(random))
          msg.AddCNAMEReply(random.ToString(), 1);
        else
          msg.AddNXReply();
      }
      else if(is_localhost_loki(msg))
      {
        size_t counter = 0;
        // I don't think this is ever hit
        llarp::LogInfo("localhost.loki CNAME write me!");
        /*
        context->ForEachService(
            [&](const std::string &,
                const std::unique_ptr< service::Endpoint > &service) -> bool {
              service::Address addr = service->GetIdentity().pub.Addr();
              msg.AddCNAMEReply(addr.ToString(), 1);
              ++counter;
              return true;
            });
        */
        if(counter == 0)
          msg.AddNXReply();
      }
      else
        msg.AddNXReply();
      reply(msg);
    }
    else if(msg.questions[0].qtype == dns::qTypeA)
    {
      llarp::service::Address addr;
      // on MacOS this is a typeA query
      if(is_random_snode(msg))
      {
        RouterID random;
        if(router->GetRandomGoodRouter(random))
          msg.AddCNAMEReply(random.ToString(), 1);
        else
          msg.AddNXReply();
      }
      else if(is_localhost_loki(msg))
      {
        llarp::LogInfo("Got is_localhost_loki message, sending back ", local_ip);
        if(local_ip.h)
        {
          msg.AddINReply(local_ip);
        }
        else
        {
          msg.AddNXReply();
        }
        /*
        size_t counter = 0;
        context->ForEachService(
            [&](const std::string &,
                const std::unique_ptr< service::Endpoint > &service) -> bool {
              huint32_t ip = service->GetIfAddr();
              if(ip.h)
              {
                msg.AddINReply(ip);
                ++counter;
              }
              return true;
            });

        if(counter == 0)
          msg.AddNXReply();
        */
      }
      /*
      else if(addr.FromString(qname, ".loki"))
      {
        if(HasAddress(addr))
        {
          huint32_t ip = ObtainIPForAddr(addr, false);
          msg.AddINReply(ip);
        }
        else
        {
          dns::Message *replyMsg = new dns::Message(std::move(msg));
          return EnsurePathToService(
              addr,
              [=](const service::Address &remote, OutboundContext *ctx) {
                SendDNSReply(remote, ctx, replyMsg, reply, false);
              },
              2000);
        }
      }
      */
      // EnsurePathToSNode
      /*
      else if(addr.FromString(qname, ".snode"))
      {
        dns::Message *replyMsg = new dns::Message(std::move(msg));
        EnsurePathToSNode(addr.as_array(),
                          [=](const RouterID &remote, exit::BaseSession *s) {
                            SendDNSReply(remote, s, replyMsg, reply, true);
                          });
        return true;
      }
      */
      else
        // forward dns
        msg.AddNXReply();

      reply(msg);
    }
    else if(msg.questions[0].qtype == dns::qTypePTR)
    {
      // reverse dns
      huint32_t ip = {0};
      if(!dns::DecodePTR(msg.questions[0].qname, ip))
      {
        msg.AddNXReply();
        reply(msg);
        return true;
      }
      /*
      llarp::service::Address addr(
          ObtainAddrForIP< llarp::service::Address >(ip, true));
      if(!addr.IsZero())
      {
        msg.AddAReply(addr.ToString(".snode"));
        reply(msg);
        return true;
      }
      addr = ObtainAddrForIP< llarp::service::Address >(ip, false);
      if(!addr.IsZero())
      {
        msg.AddAReply(addr.ToString(".loki"));
        reply(msg);
        return true;
      }
      */
      msg.AddNXReply();
      reply(msg);
      return true;
    }
    else
    {
      msg.AddNXReply();
      reply(msg);
    }
    return true;
  } // end llarp_HandleHookedDNSMessage
} // end llarp namespace

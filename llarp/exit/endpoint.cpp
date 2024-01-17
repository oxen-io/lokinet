#include "endpoint.hpp"

#include <llarp/handlers/exit.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/router.hpp>

namespace llarp::exit
{
  Endpoint::Endpoint(
      const llarp::PubKey& remoteIdent,
      const std::shared_ptr<llarp::path::AbstractHopHandler>& beginPath,
      bool rewriteIP,
      huint128_t ip,
      llarp::handlers::ExitEndpoint* parent)
      : createdAt{parent->Now()}
      , parent{parent}
      , remote_signkey{remoteIdent}
      , current_path{beginPath}
      , IP{ip}
      , rewrite_source{rewriteIP}
  {
    last_active = parent->Now();
  }

  Endpoint::~Endpoint()
  {
    if (current_path)
      parent->DelEndpointInfo(current_path->RXID());
  }

  void
  Endpoint::Close()
  {
    parent->RemoveExit(this);
  }

  util::StatusObject
  Endpoint::ExtractStatus() const
  {
    auto now = parent->Now();
    util::StatusObject obj{
        {"identity", remote_signkey.ToString()},
        {"ip", IP.ToString()},
        {"txRate", tx_rate},
        {"rxRate", rx_rate},
        {"createdAt", to_json(createdAt)},
        {"exiting", !rewrite_source},
        {"looksDead", LooksDead(now)},
        {"expiresSoon", ExpiresSoon(now)},
        {"expired", IsExpired(now)}};
    return obj;
  }

  bool
  Endpoint::UpdateLocalPath(const llarp::PathID_t& nextPath)
  {
    if (!parent->UpdateEndpointPath(remote_signkey, nextPath))
      return false;
    const RouterID us{parent->GetRouter()->pubkey()};
    // TODO: is this getting a Path or a TransitHop?
    // current_path = parent->GetRouter()->path_context().GetByUpstream(us, nextPath);
    return true;
  }

  void
  Endpoint::Tick(llarp_time_t now)
  {
    (void)now;
    rx_rate = 0;
    tx_rate = 0;
  }

  bool
  Endpoint::IsExpired(llarp_time_t now) const
  {
    auto path = GetCurrentPath();
    if (path)
    {
      return path->Expired(now);
    }
    // if we don't have an underlying path we are considered expired
    return true;
  }

  bool
  Endpoint::ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const
  {
    if (current_path)
      return current_path->ExpiresSoon(now, dlt);
    return true;
  }

  bool
  Endpoint::LooksDead(llarp_time_t now, llarp_time_t timeout) const
  {
    if (ExpiresSoon(now, timeout))
      return true;
    auto path = GetCurrentPath();
    if (not path)
      return true;
    auto lastPing = path->LastRemoteActivityAt();
    if (lastPing == 0s || (now > lastPing && now - lastPing > timeout))
      return now > last_active && now - last_active > timeout;
    else if (lastPing > 0s)  // NOLINT
      return now > lastPing && now - lastPing > timeout;
    return lastPing > 0s;
  }

  /*   bool
    Endpoint::QueueOutboundTraffic(
        PathID_t path, std::vector<byte_t> buf, uint64_t counter, service::ProtocolType t)
    {
      const service::ConvoTag tag{path.as_array()};

      // current_path->send_path_control_message(std::string method, std::string body,
    std::function<void (oxen::quic::message)> func)

      if (t == service::ProtocolType::QUIC)
      {
        auto quic = parent->GetQUICTunnel();
        if (not quic)
          return false;
        tx_rate += buf.size();
        quic->receive_packet(tag, std::move(buf));
        last_active = parent->Now();
        return true;
      }
      // queue overflow
      if (m_UpstreamQueue.size() > MaxUpstreamQueueSize)
        return false;

      llarp::net::IPPacket pkt{std::move(buf)};
      if (pkt.empty())
        return false;

      if (pkt.IsV6() && parent->SupportsV6())
      {
        huint128_t dst;
        if (rewrite_source)
          dst = parent->GetIfAddr();
        else
          dst = pkt.dstv6();
        pkt.UpdateIPv6Address(IP, dst);
      }
      else if (pkt.IsV4() && !parent->SupportsV6())
      {
        huint32_t dst;
        if (rewrite_source)
          dst = net::TruncateV6(parent->GetIfAddr());
        else
          dst = pkt.dstv4();
        pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(IP)), xhtonl(dst));
      }
      else
      {
        return false;
      }
      tx_rate += pkt.size();
      m_UpstreamQueue.emplace(std::move(pkt), counter);
      last_active = parent->Now();
      return true;
    } */

  bool
  Endpoint::QueueInboundTraffic(std::vector<byte_t>, service::ProtocolType)
  {
    // TODO: this will go away with removing flush

    // if (type != service::ProtocolType::QUIC)
    // {
    //   llarp::net::IPPacket pkt{std::move(buf)};
    //   if (pkt.empty())
    //     return false;

    //   huint128_t src;
    //   if (m_RewriteSource)
    //     src = m_Parent->GetIfAddr();
    //   else
    //     src = pkt.srcv6();
    //   if (pkt.IsV6())
    //     pkt.UpdateIPv6Address(src, m_IP);
    //   else
    //     pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(src)), xhtonl(net::TruncateV6(m_IP)));

    //   buf = pkt.steal();
    // }

    // const uint8_t queue_idx = buf.size() / llarp::routing::EXIT_PAD_SIZE;
    // if (m_DownstreamQueues.find(queue_idx) == m_DownstreamQueues.end())
    //   m_DownstreamQueues.emplace(queue_idx, InboundTrafficQueue_t{});
    // auto& queue = m_DownstreamQueues.at(queue_idx);
    // if (queue.size() == 0)
    // {
    //   queue.emplace_back();
    //   queue.back().protocol = type;
    //   return queue.back().PutBuffer(std::move(buf), m_Counter++);
    // }
    // auto& msg = queue.back();
    // if (msg.Size() + buf.size() > llarp::routing::EXIT_PAD_SIZE)
    // {
    //   queue.emplace_back();
    //   queue.back().protocol = type;
    //   return queue.back().PutBuffer(std::move(buf), m_Counter++);
    // }
    // msg.protocol = type;
    // return msg.PutBuffer(std::move(buf), m_Counter++);
    return true;
  }

  bool
  Endpoint::Flush()
  {
    // flush upstream queue
    // while (m_UpstreamQueue.size())
    // {
    //   parent->QueueOutboundTraffic(const_cast<net::IPPacket&>(m_UpstreamQueue.top().pkt).steal());
    //   m_UpstreamQueue.pop();
    // }
    // flush downstream queue
    auto path = GetCurrentPath();
    bool sent = path != nullptr;
    // if (path)
    // {
    //   for (auto& item : m_DownstreamQueues)
    //   {
    //     auto& queue = item.second;
    //     while (queue.size())
    //     {
    //       auto& msg = queue.front();
    //       msg.sequence_number = path->NextSeqNo();
    //       if (path->SendRoutingMessage(msg, m_Parent->GetRouter()))
    //       {
    //         m_RxRate += msg.Size();
    //         sent = true;
    //       }
    //       queue.pop_front();
    //     }
    //   }
    // }
    // for (auto& item : m_DownstreamQueues)
    //   item.second.clear();
    return sent;
  }
}  // namespace llarp::exit

#include "path.hpp"

#include <llarp/messages/dht.hpp>
#include <llarp/messages/exit.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp::path
{
  Path::Path(
      Router* rtr,
      const std::vector<RemoteRC>& h,
      std::weak_ptr<PathSet> pathset,
      PathRole startingRoles,
      std::string shortName)
      : m_PathSet{std::move(pathset)}
      , router{*rtr}
      , _role{startingRoles}
      , m_shortName{std::move(shortName)}
  {
    hops.resize(h.size());
    size_t hsz = h.size();
    for (size_t idx = 0; idx < hsz; ++idx)
    {
      hops[idx].rc = h[idx];
      do
      {
        hops[idx].txID.Randomize();
      } while (hops[idx].txID.IsZero());

      do
      {
        hops[idx].rxID.Randomize();
      } while (hops[idx].rxID.IsZero());
    }

    for (size_t idx = 0; idx < hsz - 1; ++idx)
    {
      hops[idx].txID = hops[idx + 1].rxID;
    }
    // initialize parts of the introduction
    intro.router = hops[hsz - 1].rc.router_id();
    intro.path_id = hops[hsz - 1].txID;
    if (auto parent = m_PathSet.lock())
      EnterState(ePathBuilding, parent->Now());
  }

  bool
  Path::obtain_exit(
      SecretKey sk,
      uint64_t flag,
      std::string tx_id,
      std::function<void(oxen::quic::message m)> func)
  {
    return send_path_control_message(
        "obtain_exit",
        ObtainExitMessage::sign_and_serialize(sk, flag, std::move(tx_id)),
        std::move(func));
  }

  bool
  Path::close_exit(SecretKey sk, std::string tx_id, std::function<void(oxen::quic::message m)> func)
  {
    return send_path_control_message(
        "close_exit", CloseExitMessage::sign_and_serialize(sk, std::move(tx_id)), std::move(func));
  }

  bool
  Path::find_intro(
      const dht::Key_t& location,
      bool is_relayed,
      uint64_t order,
      std::function<void(oxen::quic::message m)> func)
  {
    return send_path_control_message(
        "find_intro", FindIntroMessage::serialize(location, is_relayed, order), std::move(func));
  }

  bool
  Path::find_name(std::string name, std::function<void(oxen::quic::message m)> func)
  {
    return send_path_control_message(
        "find_name", FindNameMessage::serialize(std::move(name)), std::move(func));
  }

  bool
  Path::find_router(std::string rid, std::function<void(oxen::quic::message m)> func)
  {
    return send_path_control_message(
        "find_router", FindRouterMessage::serialize(std::move(rid), false, false), std::move(func));
  }

  bool
  Path::send_path_control_message(
      std::string method, std::string body, std::function<void(oxen::quic::message m)> func)
  {
    std::string payload;

    {
      oxenc::bt_dict_producer btdp;
      btdp.append("BODY", body);
      btdp.append("METHOD", method);
      payload = std::move(btdp).str();
    }

    TunnelNonce nonce;
    nonce.Randomize();

    for (const auto& hop : hops)
    {
      // do a round of chacha for each hop and mutate the nonce with that hop's nonce
      crypto::xchacha20(
          reinterpret_cast<unsigned char*>(payload.data()), payload.size(), hop.shared, nonce);

      nonce ^= hop.nonceXOR;
    }

    oxenc::bt_dict_producer outer_dict;
    outer_dict.append("NONCE", nonce.ToView());
    outer_dict.append("PATHID", TXID().ToView());
    outer_dict.append("PAYLOAD", payload);

    return router.send_control_message(
        upstream(),
        "path_control",
        std::move(outer_dict).str(),
        [response_cb = std::move(func)](oxen::quic::message m) {
          if (m)
          {
            // do path hop logic here
          }
        });
  }

  bool
  Path::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    if (not m_UpstreamReplayFilter.Insert(Y))
      return false;
    return AbstractHopHandler::HandleUpstream(X, Y, r);
  }

  bool
  Path::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    if (not m_DownstreamReplayFilter.Insert(Y))
      return false;
    return AbstractHopHandler::HandleDownstream(X, Y, r);
  }

  RouterID
  Path::Endpoint() const
  {
    return hops[hops.size() - 1].rc.router_id();
  }

  PubKey
  Path::EndpointPubKey() const
  {
    return hops[hops.size() - 1].rc.router_id();
  }

  PathID_t
  Path::TXID() const
  {
    return hops[0].txID;
  }

  PathID_t
  Path::RXID() const
  {
    return hops[0].rxID;
  }

  bool
  Path::IsReady() const
  {
    if (Expired(llarp::time_now_ms()))
      return false;
    return intro.latency > 0s && _status == ePathEstablished;
  }

  bool
  Path::is_endpoint(const RouterID& r, const PathID_t& id) const
  {
    return hops[hops.size() - 1].rc.router_id() == r && hops[hops.size() - 1].txID == id;
  }

  RouterID
  Path::upstream() const
  {
    return hops[0].rc.router_id();
  }

  const std::string&
  Path::ShortName() const
  {
    return m_shortName;
  }

  std::string
  Path::HopsString() const
  {
    std::string hops_str;
    hops_str.reserve(hops.size() * 62);  // 52 for the pkey, 6 for .snode, 4 for the ' -> ' joiner
    for (const auto& hop : hops)
    {
      if (!hops.empty())
        hops_str += " -> ";
      hops_str += hop.rc.router_id().ToView();
    }
    return hops_str;
  }

  void
  Path::EnterState(PathStatus st, llarp_time_t now)
  {
    if (st == ePathFailed)
    {
      _status = st;
      return;
    }
    if (st == ePathExpired && _status == ePathBuilding)
    {
      _status = st;
      if (auto parent = m_PathSet.lock())
      {
        parent->HandlePathBuildTimeout(shared_from_this());
      }
    }
    else if (st == ePathBuilding)
    {
      LogInfo("path ", name(), " is building");
      buildStarted = now;
    }
    else if (st == ePathEstablished && _status == ePathBuilding)
    {
      LogInfo("path ", name(), " is built, took ", ToString(now - buildStarted));
    }
    else if (st == ePathTimeout && _status == ePathEstablished)
    {
      LogInfo("path ", name(), " died");
      _status = st;
      if (auto parent = m_PathSet.lock())
      {
        parent->HandlePathDied(shared_from_this());
      }
    }
    else if (st == ePathEstablished && _status == ePathTimeout)
    {
      LogInfo("path ", name(), " reanimated");
    }
    else if (st == ePathIgnore)
    {
      LogInfo("path ", name(), " ignored");
    }
    _status = st;
  }

  util::StatusObject
  PathHopConfig::ExtractStatus() const
  {
    util::StatusObject obj{
        {"ip", rc.addr().to_string()},
        {"lifetime", to_json(lifetime)},
        {"router", rc.router_id().ToHex()},
        {"txid", txID.ToHex()},
        {"rxid", rxID.ToHex()}};
    return obj;
  }

  util::StatusObject
  Path::ExtractStatus() const
  {
    auto now = llarp::time_now_ms();

    util::StatusObject obj{
        {"intro", intro.ExtractStatus()},
        {"lastRecvMsg", to_json(m_LastRecvMessage)},
        {"lastLatencyTest", to_json(m_LastLatencyTestTime)},
        {"buildStarted", to_json(buildStarted)},
        {"expired", Expired(now)},
        {"expiresSoon", ExpiresSoon(now)},
        {"expiresAt", to_json(ExpireTime())},
        {"ready", IsReady()},
        {"txRateCurrent", m_LastTXRate},
        {"rxRateCurrent", m_LastRXRate},
        {"replayTX", m_UpstreamReplayFilter.Size()},
        {"replayRX", m_DownstreamReplayFilter.Size()},
        {"hasExit", SupportsAnyRoles(ePathRoleExit)}};

    std::vector<util::StatusObject> hopsObj;
    std::transform(
        hops.begin(),
        hops.end(),
        std::back_inserter(hopsObj),
        [](const auto& hop) -> util::StatusObject { return hop.ExtractStatus(); });
    obj["hops"] = hopsObj;

    switch (_status)
    {
      case ePathBuilding:
        obj["status"] = "building";
        break;
      case ePathEstablished:
        obj["status"] = "established";
        break;
      case ePathTimeout:
        obj["status"] = "timeout";
        break;
      case ePathExpired:
        obj["status"] = "expired";
        break;
      case ePathFailed:
        obj["status"] = "failed";
        break;
      case ePathIgnore:
        obj["status"] = "ignored";
        break;
      default:
        obj["status"] = "unknown";
        break;
    }
    return obj;
  }

  void
  Path::Rebuild()
  {
    if (auto parent = m_PathSet.lock())
    {
      std::vector<RemoteRC> new_hops;

      for (const auto& hop : hops)
        new_hops.emplace_back(hop.rc);

      LogInfo(name(), " rebuilding on ", ShortName());
      parent->Build(new_hops);
    }
  }

  bool
  Path::SendLatencyMessage(Router*)
  {
    // const auto now = r->now();
    // // send path latency test
    // routing::PathLatencyMessage latency{};
    // latency.sent_time = randint();
    // latency.sequence_number = NextSeqNo();
    // m_LastLatencyTestID = latency.sent_time;
    // m_LastLatencyTestTime = now;
    // LogDebug(name(), " send latency test id=", latency.sent_time);
    // if (not SendRoutingMessage(latency, r))
    //   return false;
    // FlushUpstream(r);
    return true;
  }

  bool
  Path::update_exit(uint64_t)
  {
    // TODO: do we still want this concept?
    return false;
  }

  void
  Path::Tick(llarp_time_t now, Router* r)
  {
    if (Expired(now))
      return;

    m_LastRXRate = m_RXRate;
    m_LastTXRate = m_TXRate;

    m_RXRate = 0;
    m_TXRate = 0;

    if (_status == ePathBuilding)
    {
      if (buildStarted == 0s)
        return;
      if (now >= buildStarted)
      {
        const auto dlt = now - buildStarted;
        if (dlt >= path::BUILD_TIMEOUT)
        {
          LogWarn(name(), " waited for ", ToString(dlt), " and no path was built");
          r->router_profiling().MarkPathFail(this);
          EnterState(ePathExpired, now);
          return;
        }
      }
    }
    // check to see if this path is dead
    if (_status == ePathEstablished)
    {
      auto dlt = now - m_LastLatencyTestTime;
      if (dlt > path::LATENCY_INTERVAL && m_LastLatencyTestID == 0)
      {
        SendLatencyMessage(r);
        // latency test FEC
        r->loop()->call_later(2s, [self = shared_from_this(), r]() {
          if (self->m_LastLatencyTestID)
            self->SendLatencyMessage(r);
        });
        return;
      }
      dlt = now - m_LastRecvMessage;
      if (dlt >= path::ALIVE_TIMEOUT)
      {
        LogWarn(name(), " waited for ", ToString(dlt), " and path looks dead");
        r->router_profiling().MarkPathFail(this);
        EnterState(ePathTimeout, now);
      }
    }
    if (_status == ePathIgnore and now - m_LastRecvMessage >= path::ALIVE_TIMEOUT)
    {
      // clean up this path as we dont use it anymore
      EnterState(ePathExpired, now);
    }
  }

  void
  Path::HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, Router* r)
  {
    for (const auto& msg : msgs)
    {
      if (r->send_data_message(upstream(), msg.bt_encode()))
      {
        m_TXRate += msg.enc.size();
      }
      else
      {
        LogDebug("failed to send upstream to ", upstream());
      }
    }
    r->TriggerPump();
  }

  void
  Path::UpstreamWork(TrafficQueue_t msgs, Router* r)
  {
    std::vector<RelayUpstreamMessage> sendmsgs(msgs.size());
    size_t idx = 0;
    for (auto& ev : msgs)
    {
      TunnelNonce n = ev.second;

      uint8_t* buf = ev.first.data();
      size_t sz = ev.first.size();

      for (const auto& hop : hops)
      {
        crypto::xchacha20(buf, sz, hop.shared, n);
        n ^= hop.nonceXOR;
      }
      auto& msg = sendmsgs[idx];
      std::memcpy(msg.enc.data(), buf, sz);
      msg.nonce = ev.second;
      msg.pathid = TXID();
      ++idx;
    }
    r->loop()->call([self = shared_from_this(), data = std::move(sendmsgs), r]() mutable {
      self->HandleAllUpstream(std::move(data), r);
    });
  }

  void
  Path::FlushUpstream(Router* r)
  {
    if (not m_UpstreamQueue.empty())
    {
      r->queue_work([self = shared_from_this(),
                     data = std::exchange(m_UpstreamQueue, {}),
                     r]() mutable { self->UpstreamWork(std::move(data), r); });
    }
  }

  void
  Path::FlushDownstream(Router* r)
  {
    if (not m_DownstreamQueue.empty())
    {
      r->queue_work([self = shared_from_this(),
                     data = std::exchange(m_DownstreamQueue, {}),
                     r]() mutable { self->DownstreamWork(std::move(data), r); });
    }
  }

  /// how long we wait for a path to become active again after it times out
  constexpr auto PathReanimationTimeout = 45s;

  bool
  Path::Expired(llarp_time_t now) const
  {
    if (_status == ePathFailed)
      return true;
    if (_status == ePathBuilding)
      return false;
    if (_status == ePathTimeout)
    {
      return now >= m_LastRecvMessage + PathReanimationTimeout;
    }
    if (_status == ePathEstablished or _status == ePathIgnore)
    {
      return now >= ExpireTime();
    }
    return true;
  }

  std::string
  Path::name() const
  {
    return fmt::format("TX={} RX={}", TXID(), RXID());
  }

  void
  Path::DownstreamWork(TrafficQueue_t msgs, Router* r)
  {
    std::vector<RelayDownstreamMessage> sendMsgs(msgs.size());
    size_t idx = 0;
    for (auto& ev : msgs)
    {
      sendMsgs[idx].nonce = ev.second;

      uint8_t* buf = ev.first.data();
      size_t sz = ev.first.size();

      for (const auto& hop : hops)
      {
        sendMsgs[idx].nonce ^= hop.nonceXOR;
        crypto::xchacha20(buf, sz, hop.shared, sendMsgs[idx].nonce);
      }

      std::memcpy(sendMsgs[idx].enc.data(), buf, sz);
      ++idx;
    }
    r->loop()->call([self = shared_from_this(), msgs = std::move(sendMsgs), r]() mutable {
      self->HandleAllDownstream(std::move(msgs), r);
    });
  }

  void
  Path::HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* /* r */)
  {
    for (const auto& msg : msgs)
    {
      const llarp_buffer_t buf{msg.enc};
      m_RXRate += buf.sz;
      // if (HandleRoutingMessage(buf, r))
      // {
      //   r->TriggerPump();
      //   m_LastRecvMessage = r->now();
      // }
    }
  }

  /** Note: this is one of two places where AbstractRoutingMessage::bt_encode() is called, the
      other of which is llarp/path/transit_hop.cpp in TransitHop::SendRoutingMessage(). For now,
      we will default to the override of ::bt_encode() that returns an std::string. The role that
      llarp_buffer_t plays here is likely superfluous, and can be replaced with either a leaner
      llarp_buffer, or just handled using strings.

      One important consideration is the frequency at which routing messages are sent, making
      superfluous copies important to optimize out here. We have to instantiate at least one
      std::string whether we pass a bt_dict_producer as a reference or create one within the
      ::bt_encode() call.

      If we decide to stay with std::strings, the function Path::HandleUpstream (along with the
      functions it calls and so on) will need to be modified to take an std::string that we can
      std::move around.
  */
  bool
  Path::SendRoutingMessage(std::string payload, Router*)
  {
    std::string buf(MAX_LINK_MSG_SIZE / 2, '\0');
    buf.insert(0, payload);

    // make nonce
    TunnelNonce N;
    N.Randomize();

    // pad smaller messages
    if (payload.size() < PAD_SIZE)
    {
      // randomize padding
      crypto::randbytes(
          reinterpret_cast<unsigned char*>(buf.data()) + payload.size(), PAD_SIZE - payload.size());
    }
    log::debug(path_cat, "Sending {}B routing message to {}", buf.size(), Endpoint());

    // TODO: path relaying here

    return true;
  }

  template <typename Samples_t>
  static llarp_time_t
  computeLatency(const Samples_t& samps)
  {
    llarp_time_t mean = 0s;
    if (samps.empty())
      return mean;
    for (const auto& samp : samps)
      mean += samp;
    return mean / samps.size();
  }
}  // namespace llarp::path

#include "path.hpp"

#include <llarp/dht/context.hpp>
#include <llarp/exit/exit_messages.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/messages/relay_status.hpp>
#include "pathbuilder.hpp"
#include "transit_hop.hpp"
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/routing/path_dht_message.hpp>
#include <llarp/routing/path_latency_message.hpp>
#include <llarp/routing/transfer_traffic_message.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/tooling/path_event.hpp>

#include <oxenc/endian.h>

#include <deque>
#include <queue>

namespace llarp::path
{
  Path::Path(
      const std::vector<RouterContact>& h,
      std::weak_ptr<PathSet> pathset,
      PathRole startingRoles,
      std::string shortName)
      : m_PathSet{std::move(pathset)}, _role{startingRoles}, m_shortName{std::move(shortName)}

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
    intro.router = hops[hsz - 1].rc.pubkey;
    intro.path_id = hops[hsz - 1].txID;
    if (auto parent = m_PathSet.lock())
      EnterState(ePathBuilding, parent->Now());
  }

  void
  Path::SetBuildResultHook(BuildResultHookFunc func)
  {
    m_BuiltHook = func;
  }

  bool
  Path::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    if (not m_UpstreamReplayFilter.Insert(Y))
      return false;
    return IHopHandler::HandleUpstream(X, Y, r);
  }

  bool
  Path::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    if (not m_DownstreamReplayFilter.Insert(Y))
      return false;
    return IHopHandler::HandleDownstream(X, Y, r);
  }

  RouterID
  Path::Endpoint() const
  {
    return hops[hops.size() - 1].rc.pubkey;
  }

  PubKey
  Path::EndpointPubKey() const
  {
    return hops[hops.size() - 1].rc.pubkey;
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
  Path::IsEndpoint(const RouterID& r, const PathID_t& id) const
  {
    return hops[hops.size() - 1].rc.pubkey == r && hops[hops.size() - 1].txID == id;
  }

  RouterID
  Path::Upstream() const
  {
    return hops[0].rc.pubkey;
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
      hops_str += RouterID(hop.rc.pubkey).ToString();
    }
    return hops_str;
  }

  bool
  Path::HandleLRSM(uint64_t status, std::array<EncryptedFrame, 8>& frames, Router* r)
  {
    uint64_t currentStatus = status;

    size_t index = 0;
    std::optional<RouterID> failedAt;
    while (index < hops.size())
    {
      if (!frames[index].DoDecrypt(hops[index].shared))
      {
        currentStatus = LR_StatusRecord::FAIL_DECRYPT_ERROR;
        failedAt = hops[index].rc.pubkey;
        break;
      }
      llarp::LogDebug("decrypted LRSM frame from ", hops[index].rc.pubkey);

      llarp_buffer_t* buf = frames[index].Buffer();
      buf->cur = buf->base + EncryptedFrameOverheadSize;

      LR_StatusRecord record;
      // successful decrypt
      if (!record.BDecode(buf))
      {
        llarp::LogWarn("malformed frame inside LRCM from ", hops[index].rc.pubkey);
        currentStatus = LR_StatusRecord::FAIL_MALFORMED_RECORD;
        failedAt = hops[index].rc.pubkey;
        break;
      }
      llarp::LogDebug("Decoded LR Status Record from ", hops[index].rc.pubkey);

      currentStatus = record.status;
      if ((record.status & LR_StatusRecord::SUCCESS) != LR_StatusRecord::SUCCESS)
      {
        if (record.status & LR_StatusRecord::FAIL_CONGESTION and index == 0)
        {
          // first hop building too fast
          failedAt = hops[index].rc.pubkey;
          break;
        }
        // failed at next hop
        if (index + 1 < hops.size())
        {
          failedAt = hops[index + 1].rc.pubkey;
        }
        break;
      }
      ++index;
    }

    if ((currentStatus & LR_StatusRecord::SUCCESS) == LR_StatusRecord::SUCCESS)
    {
      llarp::LogDebug("LR_Status message processed, path build successful");
      r->loop()->call([r, self = shared_from_this()] { self->HandlePathConfirmMessage(r); });
    }
    else
    {
      if (failedAt)
      {
        r->notify_router_event<tooling::PathBuildRejectedEvent>(Endpoint(), RXID(), *failedAt);
        LogWarn(
            Name(),
            " build failed at ",
            *failedAt,
            " status=",
            LRStatusCodeToString(currentStatus),
            " hops=",
            HopsString());
        r->router_profiling().MarkHopFail(*failedAt);
      }
      else
        r->router_profiling().MarkPathFail(this);
      llarp::LogDebug("LR_Status message processed, path build failed");

      if (currentStatus & LR_StatusRecord::FAIL_TIMEOUT)
      {
        llarp::LogDebug("Path build failed due to timeout");
      }
      else if (currentStatus & LR_StatusRecord::FAIL_CONGESTION)
      {
        llarp::LogDebug("Path build failed due to congestion");
      }
      else if (currentStatus & LR_StatusRecord::FAIL_DEST_UNKNOWN)
      {
        llarp::LogDebug(
            "Path build failed due to one or more nodes giving destination "
            "unknown");
      }
      else if (currentStatus & LR_StatusRecord::FAIL_DEST_INVALID)
      {
        llarp::LogDebug(
            "Path build failed due to one or more nodes considered an "
            "invalid destination");
        if (failedAt)
        {
          r->loop()->call([nodedb = r->node_db(), router = *failedAt]() {
            LogInfo("router ", router, " is deregistered so we remove it");
            nodedb->Remove(router);
          });
        }
      }
      else if (currentStatus & LR_StatusRecord::FAIL_CANNOT_CONNECT)
      {
        llarp::LogDebug(
            "Path build failed due to a node being unable to connect to the "
            "next hop");
      }
      else if (currentStatus & LR_StatusRecord::FAIL_MALFORMED_RECORD)
      {
        llarp::LogDebug(
            "Path build failed due to a malformed record in the build status "
            "message");
      }
      else if (currentStatus & LR_StatusRecord::FAIL_DECRYPT_ERROR)
      {
        llarp::LogDebug(
            "Path build failed due to a decrypt error in the build status "
            "message");
      }
      else
      {
        llarp::LogDebug("Path build failed for an unspecified reason");
      }
      RouterID edge{};
      if (failedAt)
        edge = *failedAt;
      r->loop()->call([r, self = shared_from_this(), edge]() {
        self->EnterState(ePathFailed, r->Now());
        if (auto parent = self->m_PathSet.lock())
        {
          parent->HandlePathBuildFailedAt(self, edge);
        }
      });
    }

    // TODO: meaningful return value?
    return true;
  }  // namespace path

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
      LogInfo("path ", Name(), " is building");
      buildStarted = now;
    }
    else if (st == ePathEstablished && _status == ePathBuilding)
    {
      LogInfo("path ", Name(), " is built, took ", ToString(now - buildStarted));
    }
    else if (st == ePathTimeout && _status == ePathEstablished)
    {
      LogInfo("path ", Name(), " died");
      _status = st;
      if (auto parent = m_PathSet.lock())
      {
        parent->HandlePathDied(shared_from_this());
      }
    }
    else if (st == ePathEstablished && _status == ePathTimeout)
    {
      LogInfo("path ", Name(), " reanimated");
    }
    else if (st == ePathIgnore)
    {
      LogInfo("path ", Name(), " ignored");
    }
    _status = st;
  }

  util::StatusObject
  PathHopConfig::ExtractStatus() const
  {
    const auto ip = net::In6ToHUInt(rc.addrs[0].ip);
    util::StatusObject obj{
        {"ip", ip.ToString()},
        {"lifetime", to_json(lifetime)},
        {"router", rc.pubkey.ToHex()},
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
      std::vector<RouterContact> newHops;
      for (const auto& hop : hops)
        newHops.emplace_back(hop.rc);
      LogInfo(Name(), " rebuilding on ", ShortName());
      parent->Build(newHops);
    }
  }

  bool
  Path::SendLatencyMessage(Router* r)
  {
    const auto now = r->Now();
    // send path latency test
    routing::PathLatencyMessage latency{};
    latency.sent_time = randint();
    latency.sequence_number = NextSeqNo();
    m_LastLatencyTestID = latency.sent_time;
    m_LastLatencyTestTime = now;
    LogDebug(Name(), " send latency test id=", latency.sent_time);
    if (not SendRoutingMessage(latency, r))
      return false;
    FlushUpstream(r);
    return true;
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
        if (dlt >= path::build_timeout)
        {
          LogWarn(Name(), " waited for ", ToString(dlt), " and no path was built");
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
      if (dlt > path::latency_interval && m_LastLatencyTestID == 0)
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
      if (dlt >= path::alive_timeout)
      {
        LogWarn(Name(), " waited for ", ToString(dlt), " and path looks dead");
        r->router_profiling().MarkPathFail(this);
        EnterState(ePathTimeout, now);
      }
    }
    if (_status == ePathIgnore and now - m_LastRecvMessage >= path::alive_timeout)
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
      if (r->SendToOrQueue(Upstream(), msg))
      {
        m_TXRate += msg.enc.size();
      }
      else
      {
        LogDebug("failed to send upstream to ", Upstream());
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
      const llarp_buffer_t buf(ev.first);
      TunnelNonce n = ev.second;
      for (const auto& hop : hops)
      {
        CryptoManager::instance()->xchacha20(buf, hop.shared, n);
        n ^= hop.nonceXOR;
      }
      auto& msg = sendmsgs[idx];
      msg.enc = buf;
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
  Path::Name() const
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
      const llarp_buffer_t buf(ev.first);
      sendMsgs[idx].nonce = ev.second;
      for (const auto& hop : hops)
      {
        sendMsgs[idx].nonce ^= hop.nonceXOR;
        CryptoManager::instance()->xchacha20(buf, hop.shared, sendMsgs[idx].nonce);
      }
      sendMsgs[idx].enc = buf;
      ++idx;
    }
    r->loop()->call([self = shared_from_this(), msgs = std::move(sendMsgs), r]() mutable {
      self->HandleAllDownstream(std::move(msgs), r);
    });
  }

  void
  Path::HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* r)
  {
    for (const auto& msg : msgs)
    {
      const llarp_buffer_t buf{msg.enc};
      m_RXRate += buf.sz;
      if (HandleRoutingMessage(buf, r))
      {
        r->TriggerPump();
        m_LastRecvMessage = r->Now();
      }
    }
  }

  bool
  Path::HandleRoutingMessage(const llarp_buffer_t& buf, Router* r)
  {
    if (!r->ParseRoutingMessageBuffer(buf, this, RXID()))
    {
      LogWarn("Failed to parse inbound routing message");
      return false;
    }
    return true;
  }

  bool
  Path::HandleUpdateExitVerifyMessage(const routing::UpdateExitVerifyMessage& msg, Router* r)
  {
    (void)r;
    if (m_UpdateExitTX && msg.tx_id == m_UpdateExitTX)
    {
      if (m_ExitUpdated)
        return m_ExitUpdated(shared_from_this());
    }
    if (m_CloseExitTX && msg.tx_id == m_CloseExitTX)
    {
      if (m_ExitClosed)
        return m_ExitClosed(shared_from_this());
    }
    return false;
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
  Path::SendRoutingMessage(const routing::AbstractRoutingMessage& msg, Router* r)
  {
    std::array<byte_t, MAX_LINK_MSG_SIZE / 2> tmp;
    llarp_buffer_t buf(tmp);
    // should help prevent bad paths with uninitialized members
    // FIXME: Why would we get uninitialized IMessages?
    if (msg.version != llarp::constants::proto_version)
      return false;

    auto bte = msg.bt_encode();
    buf.write(bte.begin(), bte.end());

    // make nonce
    TunnelNonce N;
    N.Randomize();
    buf.sz = buf.cur - buf.base;
    // pad smaller messages
    if (buf.sz < pad_size)
    {
      // randomize padding
      CryptoManager::instance()->randbytes(buf.cur, pad_size - buf.sz);
      buf.sz = pad_size;
    }
    buf.cur = buf.base;
    LogDebug(
        "send routing message ",
        msg.sequence_number,
        " with ",
        buf.sz,
        " bytes to endpoint ",
        Endpoint());
    return HandleUpstream(buf, N, r);
  }

  bool
  Path::HandlePathTransferMessage(const routing::PathTransferMessage& /*msg*/, Router* /*r*/)
  {
    LogWarn("unwarranted path transfer message on tx=", TXID(), " rx=", RXID());
    return false;
  }

  bool
  Path::HandleDataDiscardMessage(const routing::DataDiscardMessage& msg, Router* r)
  {
    MarkActive(r->Now());
    if (m_DropHandler)
      return m_DropHandler(shared_from_this(), msg.path_id, msg.sequence_number);
    return true;
  }

  bool
  Path::HandlePathConfirmMessage(Router* r)
  {
    LogDebug("Path Build Confirm, path: ", ShortName());
    const auto now = llarp::time_now_ms();
    if (_status == ePathBuilding)
    {
      // finish initializing introduction
      intro.expiry = buildStarted + hops[0].lifetime;

      r->router_profiling().MarkPathSuccess(this);

      // persist session with upstream router until the path is done
      r->PersistSessionUntil(Upstream(), intro.expiry);
      MarkActive(now);
      return SendLatencyMessage(r);
    }
    LogWarn("got unwarranted path confirm message on tx=", RXID(), " rx=", RXID());
    return false;
  }

  bool
  Path::HandlePathConfirmMessage(const routing::PathConfirmMessage& /*msg*/, Router* r)
  {
    return HandlePathConfirmMessage(r);
  }

  bool
  Path::HandleHiddenServiceFrame(const service::ProtocolFrameMessage& frame)
  {
    if (auto parent = m_PathSet.lock())
    {
      MarkActive(parent->Now());
      return m_DataHandler && m_DataHandler(shared_from_this(), frame);
    }
    return false;
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

  constexpr auto MaxLatencySamples = 8;

  bool
  Path::HandlePathLatencyMessage(const routing::PathLatencyMessage&, Router* r)
  {
    const auto now = r->Now();
    MarkActive(now);
    if (m_LastLatencyTestID)
    {
      m_LatencySamples.emplace_back(now - m_LastLatencyTestTime);

      while (m_LatencySamples.size() > MaxLatencySamples)
        m_LatencySamples.pop_front();

      intro.latency = computeLatency(m_LatencySamples);
      m_LastLatencyTestID = 0;
      EnterState(ePathEstablished, now);
      if (m_BuiltHook)
        m_BuiltHook(shared_from_this());
      m_BuiltHook = nullptr;
    }
    return true;
  }

  /// this is the Client's side of handling a DHT message. it's handled
  /// in-place.
  bool
  Path::HandleDHTMessage(const dht::AbstractDHTMessage& msg, Router* r)
  {
    MarkActive(r->Now());
    routing::PathDHTMessage reply;
    if (not r->dht()->handle_message(msg, reply.dht_msgs))
      return false;
    if (reply.dht_msgs.size())
      return SendRoutingMessage(reply, r);
    return true;
  }

  bool
  Path::HandleCloseExitMessage(const routing::CloseExitMessage& msg, Router* /*r*/)
  {
    /// allows exits to close from their end
    if (SupportsAnyRoles(ePathRoleExit | ePathRoleSVC))
    {
      if (msg.Verify(EndpointPubKey()))
      {
        LogInfo(Name(), " had its exit closed");
        _role &= ~ePathRoleExit;
        return true;
      }

      LogError(Name(), " CXM from exit with bad signature");
    }
    else
      LogError(Name(), " unwarranted CXM");
    return false;
  }

  bool
  Path::SendExitRequest(const routing::ObtainExitMessage& msg, Router* r)
  {
    LogInfo(Name(), " sending exit request to ", Endpoint());
    m_ExitObtainTX = msg.tx_id;
    return SendRoutingMessage(msg, r);
  }

  bool
  Path::SendExitClose(const routing::CloseExitMessage& msg, Router* r)
  {
    LogInfo(Name(), " closing exit to ", Endpoint());
    // mark as not exit anymore
    _role &= ~ePathRoleExit;
    return SendRoutingMessage(msg, r);
  }

  bool
  Path::HandleObtainExitMessage(const routing::ObtainExitMessage& msg, Router* r)
  {
    (void)msg;
    (void)r;
    LogError(Name(), " got unwarranted OXM");
    return false;
  }

  bool
  Path::HandleUpdateExitMessage(const routing::UpdateExitMessage& msg, Router* r)
  {
    (void)msg;
    (void)r;
    LogError(Name(), " got unwarranted UXM");
    return false;
  }

  bool
  Path::HandleRejectExitMessage(const routing::RejectExitMessage& msg, Router* r)
  {
    if (m_ExitObtainTX && msg.tx_id == m_ExitObtainTX)
    {
      if (!msg.Verify(EndpointPubKey()))
      {
        LogError(Name(), "RXM invalid signature");
        return false;
      }
      LogInfo(Name(), " ", Endpoint(), " Rejected exit");
      MarkActive(r->Now());
      return InformExitResult(llarp_time_t(msg.backoff_time));
    }
    LogError(Name(), " got unwarranted RXM");
    return false;
  }

  bool
  Path::HandleGrantExitMessage(const routing::GrantExitMessage& msg, Router* r)
  {
    if (m_ExitObtainTX && msg.tx_id == m_ExitObtainTX)
    {
      if (!msg.Verify(EndpointPubKey()))
      {
        LogError(Name(), " GXM signature failed");
        return false;
      }
      // we now can send exit traffic
      _role |= ePathRoleExit;
      LogInfo(Name(), " ", Endpoint(), " Granted exit");
      MarkActive(r->Now());
      return InformExitResult(0s);
    }
    LogError(Name(), " got unwarranted GXM");
    return false;
  }

  bool
  Path::InformExitResult(llarp_time_t B)
  {
    auto self = shared_from_this();
    bool result = true;
    for (const auto& hook : m_ObtainedExitHooks)
      result = hook(self, B) and result;
    m_ObtainedExitHooks.clear();
    return result;
  }

  bool
  Path::HandleTransferTrafficMessage(const routing::TransferTrafficMessage& msg, Router* r)
  {
    // check if we can handle exit data
    if (!SupportsAnyRoles(ePathRoleExit | ePathRoleSVC))
      return false;
    // handle traffic if we have a handler
    if (!m_ExitTrafficHandler)
      return false;
    bool sent = msg.enc_buf.size() > 0;
    auto self = shared_from_this();
    for (const auto& pkt : msg.enc_buf)
    {
      if (pkt.size() <= 8)
        continue;
      auto counter = oxenc::load_big_to_host<uint64_t>(pkt.data());
      if (m_ExitTrafficHandler(
              self, llarp_buffer_t(pkt.data() + 8, pkt.size() - 8), counter, msg.protocol))
      {
        MarkActive(r->Now());
        EnterState(ePathEstablished, r->Now());
      }
    }
    return sent;
  }

}  // namespace llarp::path

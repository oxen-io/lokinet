#include "outbound_context.hpp"

#include <llarp/router/abstractrouter.hpp>
#include "async_key_exchange.hpp"
#include "hidden_service_address_lookup.hpp"
#include "endpoint.hpp"
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/util/meta/memfn.hpp>

#include "endpoint_util.hpp"
#include "service/protocol_type.hpp"

#include <random>
#include <algorithm>

namespace llarp
{
  namespace service
  {
    bool
    OutboundContext::Stop()
    {
      markedBad = true;
      return path::Builder::Stop();
    }

    bool
    OutboundContext::IsDone(llarp_time_t now) const
    {
      (void)now;
      return AvailablePaths(path::ePathRoleAny) == 0 && ShouldRemove();
    }

    bool
    OutboundContext::ShouldBundleRC() const
    {
      return m_Endpoint->ShouldBundleRC();
    }

    bool
    OutboundContext::HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t seq)
    {
      // pick another intro
      if (dst == remoteIntro.pathID && remoteIntro.router == p->Endpoint())
      {
        LogWarn(Name(), " message ", seq, " dropped by endpoint ", p->Endpoint(), " via ", dst);
        MarkCurrentIntroBad(Now());
      }
      return true;
    }

    constexpr auto OutboundContextNumPaths = 2;

    OutboundContext::OutboundContext(const IntroSet& introset, Endpoint* parent)
        : path::Builder{parent->Router(), OutboundContextNumPaths, parent->numHops}
        , SendContext{introset.addressKeys, {}, this, parent}
        , location{introset.addressKeys.Addr().ToKey()}
        , currentIntroSet{introset}

    {
      updatingIntroSet = false;
      for (const auto& intro : introset.intros)
      {
        if (intro.expiresAt > m_NextIntro.expiresAt)
          m_NextIntro = intro;
      }

      currentConvoTag.Randomize();
    }

    OutboundContext::~OutboundContext() = default;

    /// actually swap intros
    void
    OutboundContext::SwapIntros()
    {
      if (remoteIntro != m_NextIntro)
      {
        remoteIntro = m_NextIntro;
        m_DataHandler->PutSenderFor(currentConvoTag, currentIntroSet.addressKeys, false);
        m_DataHandler->PutIntroFor(currentConvoTag, remoteIntro);
      }
    }

    bool
    OutboundContext::OnIntroSetUpdate(
        const Address&, std::optional<IntroSet> foundIntro, const RouterID& endpoint, llarp_time_t)
    {
      if (markedBad)
        return true;
      updatingIntroSet = false;
      if (foundIntro)
      {
        if (foundIntro->timestampSignedAt == 0s)
        {
          LogWarn(Name(), " got introset with zero timestamp: ", *foundIntro);
          return true;
        }
        if (currentIntroSet.timestampSignedAt > foundIntro->timestampSignedAt)
        {
          LogInfo("introset is old, dropping");
          return true;
        }

        const llarp_time_t now = Now();
        if (foundIntro->IsExpired(now))
        {
          LogError("got expired introset from lookup from ", endpoint);
          return true;
        }
        currentIntroSet = *foundIntro;
      }
      else
      {
        ++m_LookupFails;
        LogWarn(Name(), " failed to look up introset, fails=", m_LookupFails);
      }
      return true;
    }

    bool
    OutboundContext::ReadyToSend() const
    {
      if (markedBad)
        return false;
      if (remoteIntro.router.IsZero())
        return false;
      return GetPathByRouter(remoteIntro.router) != nullptr;
    }

    void
    OutboundContext::ShiftIntroRouter(const RouterID r)
    {
      const auto now = Now();
      Introduction selectedIntro;
      for (const auto& intro : currentIntroSet.intros)
      {
        if (intro.expiresAt > selectedIntro.expiresAt && intro.router != r)
        {
          selectedIntro = intro;
        }
      }
      if (selectedIntro.router.IsZero() || selectedIntro.ExpiresSoon(now))
        return;
      LogWarn(Name(), " shfiting intro off of ", r, " to ", RouterID(selectedIntro.router));
      m_NextIntro = selectedIntro;
    }

    void
    OutboundContext::HandlePathBuildTimeout(path::Path_ptr p)
    {
      ShiftIntroRouter(p->Endpoint());
      path::Builder::HandlePathBuildTimeout(p);
    }

    void
    OutboundContext::HandlePathBuildFailedAt(path::Path_ptr p, RouterID hop)
    {
      if (p->Endpoint() == hop)
      {
        // shift intro when we fail at the pivot
        ShiftIntroRouter(p->Endpoint());
      }
      path::Builder::HandlePathBuildFailedAt(p, hop);
    }

    void
    OutboundContext::HandlePathBuilt(path::Path_ptr p)
    {
      path::Builder::HandlePathBuilt(p);
      /// don't use it if we are marked bad
      if (markedBad)
        return;
      p->SetDataHandler(util::memFn(&OutboundContext::HandleHiddenServiceFrame, this));
      p->SetDropHandler(util::memFn(&OutboundContext::HandleDataDrop, this));
      // we now have a path to the next intro, swap intros
      if (p->Endpoint() == m_NextIntro.router or p->Endpoint() == remoteIntro.router)
        SwapIntros();
      else
      {
        LogInfo(Name(), " built to non aligned router: ", p->Endpoint());
      }
    }

    void
    OutboundContext::AsyncGenIntro(const llarp_buffer_t& payload, ProtocolType t)
    {
      if (sentIntro)
        return;
      if (remoteIntro.router.IsZero())
      {
        LogWarn(Name(), " dropping intro frame we have no intro ready yet");
        return;
      }

      auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
      if (path == nullptr)
      {
        // try parent as fallback
        path = m_Endpoint->GetPathByRouter(remoteIntro.router);
        if (path == nullptr)
        {
          if (!BuildCooldownHit(Now()))
            BuildOneAlignedTo(remoteIntro.router);
          LogWarn(Name(), " dropping intro frame, no path to ", remoteIntro.router);
          return;
        }
      }
      sentIntro = true;
      auto frame = std::make_shared<ProtocolFrame>();
      auto ex = std::make_shared<AsyncKeyExchange>(
          m_Endpoint->Loop(),
          remoteIdent,
          m_Endpoint->GetIdentity(),
          currentIntroSet.sntrupKey,
          remoteIntro,
          m_DataHandler,
          currentConvoTag,
          t);

      ex->hook = [self = shared_from_this(), path](auto frame) {
        self->Send(std::move(frame), path);
      };

      ex->msg.PutBuffer(payload);
      ex->msg.introReply = path->intro;
      frame->F = ex->msg.introReply.pathID;
      frame->R = 0;
      m_Endpoint->Router()->QueueWork([ex, frame] { return AsyncKeyExchange::Encrypt(ex, frame); });
      LogInfo("send intro frame");
    }

    std::string
    OutboundContext::Name() const
    {
      return "OBContext:" + m_Endpoint->Name() + "-"
          + currentIntroSet.addressKeys.Addr().ToString();
    }

    void
    OutboundContext::UpdateIntroSet()
    {
      constexpr auto IntrosetUpdateInterval = 10s;
      const auto now = Now();
      if (updatingIntroSet or markedBad or now < m_LastIntrosetUpdateAt + IntrosetUpdateInterval)
        return;
      LogInfo(Name(), " updating introset");
      m_LastIntrosetUpdateAt = now;
      const auto addr = currentIntroSet.addressKeys.Addr();
      // we want to use the parent endpoint's paths because outbound context
      // does not implement path::PathSet::HandleGotIntroMessage
      const auto paths = GetManyPathsWithUniqueEndpoints(m_Endpoint, 2);
      uint64_t relayOrder = 0;
      for (const auto& path : paths)
      {
        HiddenServiceAddressLookup* job = new HiddenServiceAddressLookup(
            m_Endpoint,
            util::memFn(&OutboundContext::OnIntroSetUpdate, shared_from_this()),
            location,
            PubKey{addr.as_array()},
            path->Endpoint(),
            relayOrder,
            m_Endpoint->GenTXID(),
            5s);
        relayOrder++;
        if (job->SendRequestViaPath(path, m_Endpoint->Router()))
          updatingIntroSet = true;
      }
    }

    util::StatusObject
    OutboundContext::ExtractStatus() const
    {
      auto obj = path::Builder::ExtractStatus();
      obj["estimatedRTT"] = to_json(estimatedRTT);
      obj["currentConvoTag"] = currentConvoTag.ToHex();
      obj["remoteIntro"] = remoteIntro.ExtractStatus();
      obj["sessionCreatedAt"] = to_json(createdAt);
      obj["lastGoodSend"] = to_json(lastGoodSend);
      obj["seqno"] = sequenceNo;
      obj["markedBad"] = markedBad;
      obj["lastShift"] = to_json(lastShift);
      obj["remoteIdentity"] = remoteIdent.Addr().ToString();
      obj["currentRemoteIntroset"] = currentIntroSet.ExtractStatus();
      obj["nextIntro"] = m_NextIntro.ExtractStatus();

      std::transform(
          m_BadIntros.begin(),
          m_BadIntros.end(),
          std::back_inserter(obj["badIntros"]),
          [](const auto& item) -> util::StatusObject { return item.first.ExtractStatus(); });
      return obj;
    }

    void
    OutboundContext::KeepAlive()
    {
      Encrypted<64> tmp;
      tmp.Randomize();
      SendPacketToRemote(tmp, ProtocolType::Control);
    }

    bool
    OutboundContext::Pump(llarp_time_t now)
    {
      // we are probably dead af
      if (m_LookupFails > 16 || m_BuildFails > 10)
        return true;

      constexpr auto InboundTrafficTimeout = 5s;

      if (ReadyToSend() and remoteIntro.router.IsZero())
      {
        SwapIntros();
      }

      if (m_GotInboundTraffic and m_LastInboundTraffic + InboundTrafficTimeout <= now)
      {
        if (std::chrono::abs(now - lastGoodSend) < InboundTrafficTimeout)
        {
          // timeout on other side
          MarkCurrentIntroBad(now);
        }
      }

      // check for expiration
      if (remoteIntro.ExpiresSoon(now))
      {
        UpdateIntroSet();
        // shift intro if it expires "soon"
        if (ShiftIntroduction())
          SwapIntros();  // swap intros if we shifted
      }
      // lookup router in intro if set and unknown
      m_Endpoint->EnsureRouterIsKnown(remoteIntro.router);
      // expire bad intros
      auto itr = m_BadIntros.begin();
      while (itr != m_BadIntros.end())
      {
        if (now > itr->second && now - itr->second > path::default_lifetime)
          itr = m_BadIntros.erase(itr);
        else
          ++itr;
      }

      if (ReadyToSend() and m_ReadyHook)
      {
        KeepAlive();
        const auto path = GetPathByRouter(remoteIntro.router);
        if (not path)
        {
          LogWarn(Name(), " ready but no path to ", remoteIntro.router, " ???");
          return false;
        }
        const auto rtt = (path->intro.latency + remoteIntro.latency) * 2;
        m_router->loop()->call_later(
            rtt, [rtt, self = shared_from_this(), hook = std::move(m_ReadyHook)]() {
              LogInfo(
                  self->Name(),
                  " is ready, RTT is measured as ",
                  self->estimatedRTT,
                  " approximated as ",
                  rtt,
                  " delta=",
                  rtt - self->estimatedRTT);
              hook(self.get());
            });
      }

      // if we are dead return true so we are removed
      return lastGoodSend > 0s ? (now >= lastGoodSend && now - lastGoodSend > sendTimeout)
                               : (now >= createdAt && now - createdAt > connectTimeout);
    }

    void
    OutboundContext::SetReadyHook(std::function<void(OutboundContext*)> hook, llarp_time_t timeout)
    {
      if (m_ReadyHook)
        return;
      m_ReadyHook = hook;
      m_router->loop()->call_later(timeout, [this]() {
        if (m_ReadyHook)
          m_ReadyHook(nullptr);
        m_ReadyHook = nullptr;
      });
    }

    std::optional<std::vector<RouterContact>>
    OutboundContext::GetHopsForBuild()
    {
      if (m_NextIntro.router.IsZero())
      {
        ShiftIntroduction(false);
      }
      if (m_NextIntro.router.IsZero())
        return std::nullopt;
      return GetHopsAlignedToForBuild(m_NextIntro.router, m_Endpoint->SnodeBlacklist());
    }

    bool
    OutboundContext::ShouldBuildMore(llarp_time_t now) const
    {
      if (markedBad || not path::Builder::ShouldBuildMore(now))
        return false;
      if (NumInStatus(path::ePathBuilding) >= numDesiredPaths)
        return false;
      llarp_time_t t = 0s;
      ForEachPath([&t](path::Path_ptr path) {
        if (path->IsReady())
          t = std::max(path->ExpireTime(), t);
      });
      return t >= now + path::default_lifetime / 4;
    }

    void
    OutboundContext::MarkCurrentIntroBad(llarp_time_t now)
    {
      MarkIntroBad(remoteIntro, now);
    }

    void
    OutboundContext::MarkIntroBad(const Introduction& intro, llarp_time_t now)
    {
      // insert bad intro
      m_BadIntros[intro] = now;
    }

    bool
    OutboundContext::ShiftIntroduction(bool rebuild)
    {
      bool success = false;
      auto now = Now();
      if (now - lastShift < MIN_SHIFT_INTERVAL)
        return false;
      bool shifted = false;
      std::vector<Introduction> intros = currentIntroSet.intros;
      if (intros.size() > 1)
      {
        std::shuffle(intros.begin(), intros.end(), CSRNG{});
      }

      // to find a intro on the same router as before that is newer
      for (const auto& intro : intros)
      {
        if (intro.ExpiresSoon(now))
          continue;
        if (m_Endpoint->SnodeBlacklist().count(intro.router))
          continue;
        if (m_BadIntros.find(intro) == m_BadIntros.end() && remoteIntro.router == intro.router)
        {
          if (intro.expiresAt > m_NextIntro.expiresAt)
          {
            success = true;
            m_NextIntro = intro;
            return true;
          }
        }
      }
      if (!success)
      {
        /// pick newer intro not on same router
        for (const auto& intro : intros)
        {
          if (m_Endpoint->SnodeBlacklist().count(intro.router))
            continue;
          m_Endpoint->EnsureRouterIsKnown(intro.router);
          if (intro.ExpiresSoon(now))
            continue;
          if (m_BadIntros.find(intro) == m_BadIntros.end() && m_NextIntro != intro)
          {
            if (intro.expiresAt > m_NextIntro.expiresAt)
            {
              shifted = intro.router != m_NextIntro.router;
              m_NextIntro = intro;
              success = true;
            }
          }
        }
      }
      if (m_NextIntro.router.IsZero())
        return false;
      if (shifted)
        lastShift = now;
      if (rebuild && !BuildCooldownHit(Now()))
        BuildOneAlignedTo(m_NextIntro.router);
      return success;
    }

    void
    OutboundContext::HandlePathDied(path::Path_ptr path)
    {
      // unconditionally update introset
      UpdateIntroSet();
      const RouterID endpoint(path->Endpoint());
      // if a path to our current intro died...
      if (endpoint == remoteIntro.router)
      {
        // figure out how many paths to this router we have
        size_t num = 0;
        ForEachPath([&](const path::Path_ptr& p) {
          if (p->Endpoint() == endpoint && p->IsReady())
            ++num;
        });
        // if we have more than two then we are probably fine
        if (num > 2)
          return;
        // if we have one working one ...
        if (num == 1)
        {
          num = 0;
          ForEachPath([&](const path::Path_ptr& p) {
            if (p->Endpoint() == endpoint)
              ++num;
          });
          // if we have 2 or more established or pending don't do anything
          if (num > 2)
            return;
          BuildOneAlignedTo(endpoint);
        }
        else if (num == 0)
        {
          // we have no paths to this router right now
          // hop off it
          Introduction picked;
          // get the latest intro that isn't on that endpoint
          for (const auto& intro : currentIntroSet.intros)
          {
            if (intro.router == endpoint)
              continue;
            if (intro.expiresAt > picked.expiresAt)
              picked = intro;
          }
          // we got nothing
          if (picked.router.IsZero())
          {
            return;
          }
          m_NextIntro = picked;
          // check if we have a path to this router
          num = 0;
          ForEachPath([&](const path::Path_ptr& p) {
            // don't count timed out paths
            if (p->Status() != path::ePathTimeout && p->Endpoint() == m_NextIntro.router)
              ++num;
          });
          // build a path if one isn't already pending build or established
          BuildOneAlignedTo(m_NextIntro.router);
        }
      }
    }

    bool
    OutboundContext::HandleHiddenServiceFrame(path::Path_ptr p, const ProtocolFrame& frame)
    {
      m_LastInboundTraffic = m_Endpoint->Now();
      m_GotInboundTraffic = true;
      if (frame.R)
      {
        // handle discard
        ServiceInfo si;
        if (!m_Endpoint->GetSenderFor(frame.T, si))
        {
          LogWarn("no sender for T=", frame.T);
          return false;
        }
        // verify source
        if (!frame.Verify(si))
        {
          LogWarn("signature verification failed, T=", frame.T);
          return false;
        }
        // remove convotag it doesn't exist
        LogWarn("remove convotag T=", frame.T, " R=", frame.R);

        AuthResult result{AuthResultCode::eAuthFailed, "unknown reason"};
        if (const auto maybe = AuthResultCodeFromInt(frame.R))
          result.code = *maybe;
        SharedSecret sessionKey{};
        if (m_DataHandler->GetCachedSessionKeyFor(frame.T, sessionKey))
        {
          ProtocolMessage msg{};
          if (frame.DecryptPayloadInto(sessionKey, msg))
          {
            if (msg.proto == ProtocolType::Auth and not msg.payload.empty())
            {
              result.reason = std::string{
                  reinterpret_cast<const char*>(msg.payload.data()), msg.payload.size()};
            }
          }
        }

        m_Endpoint->RemoveConvoTag(frame.T);
        if (authResultListener)
        {
          authResultListener(result);
          authResultListener = nullptr;
        }
        return true;
      }
      std::function<void(std::shared_ptr<ProtocolMessage>)> hook = nullptr;
      if (authResultListener)
      {
        std::function<void(AuthResult)> handler = authResultListener;
        authResultListener = nullptr;
        hook = [handler](std::shared_ptr<ProtocolMessage> msg) {
          AuthResult result{AuthResultCode::eAuthAccepted, "OK"};
          if (msg->proto == ProtocolType::Auth and not msg->payload.empty())
          {
            result.reason = std::string{
                reinterpret_cast<const char*>(msg->payload.data()), msg->payload.size()};
          }
          handler(result);
        };
      }
      const auto& ident = m_Endpoint->GetIdentity();
      if (not frame.AsyncDecryptAndVerify(m_Endpoint->Loop(), p, ident, m_Endpoint, hook))
      {
        // send reset convo tag message
        LogError("failed to decrypt and verify frame");
        ProtocolFrame f;
        f.R = 1;
        f.T = frame.T;
        f.F = p->intro.pathID;

        f.Sign(ident);
        {
          LogWarn("invalidating convotag T=", frame.T);
          m_Endpoint->RemoveConvoTag(frame.T);
          m_Endpoint->m_SendQueue.tryPushBack(
              SendEvent_t{std::make_shared<routing::PathTransferMessage>(f, frame.F), p});
        }
      }
      return true;
    }

    void
    OutboundContext::SendPacketToRemote(const llarp_buffer_t& buf, service::ProtocolType t)
    {
      AsyncEncryptAndSendTo(buf, t);
    }

  }  // namespace service

}  // namespace llarp

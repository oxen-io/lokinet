#pragma once

#include <llarp/path/pathbuilder.hpp>
#include "sendcontext.hpp"
#include <llarp/util/status.hpp>

#include <unordered_map>
#include <unordered_set>

namespace llarp
{
  namespace service
  {
    struct AsyncKeyExchange;
    struct Endpoint;

    /// context needed to initiate an outbound hidden service session
    struct OutboundContext : public path::Builder,
                             public SendContext,
                             public std::enable_shared_from_this<OutboundContext>
    {
      OutboundContext(const IntroSet& introSet, Endpoint* parent);

      ~OutboundContext() override;

      void
      Tick(llarp_time_t now) override;

      util::StatusObject
      ExtractStatus() const;

      void
      BlacklistSNode(const RouterID) override{};

      bool
      ShouldBundleRC() const override;

      path::PathSet_ptr
      GetSelf() override
      {
        return shared_from_this();
      }

      std::weak_ptr<path::PathSet>
      GetWeak() override
      {
        return weak_from_this();
      }

      Address
      Addr() const;

      bool
      Stop() override;

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      void
      HandlePathDied(path::Path_ptr p) override;

      /// set to true if we are updating the remote introset right now
      bool updatingIntroSet;

      /// update the current selected intro to be a new best introduction
      /// return true if we have changed intros
      bool
      ShiftIntroduction(bool rebuild = true) override;

      /// shift the intro off the current router it is using
      void
      ShiftIntroRouter(const RouterID remote) override;

      /// mark the current remote intro as bad
      void
      MarkCurrentIntroBad(llarp_time_t now) override;

      void
      MarkIntroBad(const Introduction& marked, llarp_time_t now);

      /// return true if we are ready to send
      bool
      ReadyToSend() const;

      void
      AddReadyHook(std::function<void(OutboundContext*)> readyHook, llarp_time_t timeout);

      /// for exits
      void
      SendPacketToRemote(const llarp_buffer_t&, ProtocolType t) override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      /// pump internal state
      /// return true to mark as dead
      bool
      Pump(llarp_time_t now);

      /// return true if it's safe to remove ourselves
      bool
      IsDone(llarp_time_t now) const;

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t dlt);

      void
      AsyncGenIntro(const llarp_buffer_t& payload, ProtocolType t) override;

      /// issues a lookup to find the current intro set of the remote service
      void
      UpdateIntroSet() override;

      void
      HandlePathBuilt(path::Path_ptr path) override;

      void
      HandlePathBuildTimeout(path::Path_ptr path) override;

      void
      HandlePathBuildFailedAt(path::Path_ptr path, RouterID hop) override;

      std::optional<std::vector<RouterContact>>
      GetHopsForBuild() override;

      bool
      HandleHiddenServiceFrame(path::Path_ptr p, const ProtocolFrameMessage& frame);

      std::string
      Name() const override;

      void
      KeepAlive();

      bool
      ShouldKeepAlive(llarp_time_t now) const;

      const IntroSet&
      GetCurrentIntroSet() const
      {
        return currentIntroSet;
      }

      llarp_time_t
      RTT() const;

      bool
      OnIntroSetUpdate(
          const Address& addr,
          std::optional<IntroSet> i,
          const RouterID& endpoint,
          llarp_time_t,
          uint64_t relayOrder);

     private:
      /// swap remoteIntro with next intro
      void
      SwapIntros();

      bool
      IntroGenerated() const override;
      bool
      IntroSent() const override;

      const dht::Key_t location;
      const Address addr;
      uint64_t m_UpdateIntrosetTX = 0;
      IntroSet currentIntroSet;
      Introduction m_NextIntro;
      llarp_time_t lastShift = 0s;
      uint16_t m_LookupFails = 0;
      uint16_t m_BuildFails = 0;
      llarp_time_t m_LastInboundTraffic = 0s;
      bool m_GotInboundTraffic = false;
      bool generatedIntro = false;
      bool sentIntro = false;
      std::vector<std::function<void(OutboundContext*)>> m_ReadyHooks;
      llarp_time_t m_LastIntrosetUpdateAt = 0s;
      llarp_time_t m_LastKeepAliveAt = 0s;
    };
  }  // namespace service

}  // namespace llarp

#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/path/abstracthophandler.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/queue.hpp>

namespace llarp
{
  struct LR_CommitRecord;

  namespace dht
  {
    struct GotIntroMessage;
  }

  namespace path
  {
    struct TransitHopInfo
    {
      TransitHopInfo() = default;
      TransitHopInfo(const RouterID& down, const LR_CommitRecord& record);

      PathID_t txID, rxID;
      RouterID upstream;
      RouterID downstream;

      std::string
      ToString() const;
    };

    inline bool
    operator==(const TransitHopInfo& lhs, const TransitHopInfo& rhs)
    {
      return std::tie(lhs.txID, lhs.rxID, lhs.upstream, lhs.downstream)
          == std::tie(rhs.txID, rhs.rxID, rhs.upstream, rhs.downstream);
    }

    inline bool
    operator!=(const TransitHopInfo& lhs, const TransitHopInfo& rhs)
    {
      return !(lhs == rhs);
    }

    inline bool
    operator<(const TransitHopInfo& lhs, const TransitHopInfo& rhs)
    {
      return std::tie(lhs.txID, lhs.rxID, lhs.upstream, lhs.downstream)
          < std::tie(rhs.txID, rhs.rxID, rhs.upstream, rhs.downstream);
    }

    struct TransitHop : public AbstractHopHandler, std::enable_shared_from_this<TransitHop>
    {
      TransitHop();

      TransitHopInfo info;
      SharedSecret pathKey;
      ShortHash nonceXOR;
      llarp_time_t started = 0s;
      // 10 minutes default
      llarp_time_t lifetime = DEFAULT_LIFETIME;
      llarp_proto_version_t version;
      llarp_time_t m_LastActivity = 0s;

      PathID_t
      RXID() const override
      {
        return info.rxID;
      }

      void
      Stop();

      bool destroy = false;

      bool
      operator<(const TransitHop& other) const
      {
        return info < other.info;
      }

      bool
      IsEndpoint(const RouterID& us) const
      {
        return info.upstream == us;
      }

      llarp_time_t
      ExpireTime() const;

      llarp_time_t
      LastRemoteActivityAt() const override
      {
        return m_LastActivity;
      }

      std::string
      ToString() const;

      bool
      Expired(llarp_time_t now) const override;

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const override
      {
        return now >= ExpireTime() - dlt;
      }

      bool
      send_path_control_message(
          std::string method,
          std::string body,
          std::function<void(oxen::quic::message m)> func) override;

      // send routing message when end of path
      bool
      SendRoutingMessage(const routing::AbstractRoutingMessage& msg, Router* r) override;

      bool
      HandleDHTMessage(const dht::AbstractDHTMessage& msg, Router* r) override;

      void
      FlushUpstream(Router* r) override;

      void
      FlushDownstream(Router* r) override;

      void
      QueueDestroySelf(Router* r);

     protected:
      void
      UpstreamWork(TrafficQueue_t queue, Router* r) override;

      void
      DownstreamWork(TrafficQueue_t queue, Router* r) override;

      void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, Router* r) override;

      void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* r) override;

     private:
      void
      SetSelfDestruct();

      std::set<std::shared_ptr<TransitHop>, ComparePtr<std::shared_ptr<TransitHop>>> m_FlushOthers;
      thread::Queue<RelayUpstreamMessage> m_UpstreamGather;
      thread::Queue<RelayDownstreamMessage> m_DownstreamGather;
      std::atomic<uint32_t> m_UpstreamWorkCounter;
      std::atomic<uint32_t> m_DownstreamWorkCounter;
    };
  }  // namespace path

  template <>
  constexpr inline bool IsToStringFormattable<path::TransitHop> = true;
  template <>
  constexpr inline bool IsToStringFormattable<path::TransitHopInfo> = true;

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::path::TransitHopInfo>
  {
    std::size_t
    operator()(llarp::path::TransitHopInfo const& a) const
    {
      hash<llarp::RouterID> RHash{};
      hash<llarp::PathID_t> PHash{};
      return RHash(a.upstream) ^ RHash(a.downstream) ^ PHash(a.txID) ^ PHash(a.rxID);
    }
  };
}  // namespace std

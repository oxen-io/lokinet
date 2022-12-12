#include "endpoint.hpp"
#include "endpoint_state.hpp"
#include "endpoint_util.hpp"
#include "hidden_service_address_lookup.hpp"
#include "auth.hpp"
#include "llarp/util/logging.hpp"
#include "outbound_context.hpp"
#include "protocol.hpp"
#include "info.hpp"
#include "protocol_type.hpp"

#include <llarp/net/ip.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/dht/context.hpp>
#include <llarp/dht/key.hpp>
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/findname.hpp>
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/dht/messages/gotname.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/route_poker.hpp>
#include <llarp/routing/dht_message.hpp>
#include <llarp/routing/path_transfer_message.hpp>

#include <llarp/util/str.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/tooling/dht_event.hpp>
#include <llarp/quic/server.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/util/priority_queue.hpp>

#include <optional>
#include <type_traits>
#include <utility>
#include <uvw.hpp>
#include <variant>

namespace llarp
{
  namespace service
  {
    static auto logcat = log::Cat("endpoint");

    Endpoint::Endpoint(AbstractRouter* r, Context* parent)
        : path::Builder{r, 3, path::default_len}
        , context{parent}
        , m_InboundTrafficQueue{512}
        , m_SendQueue{512}
        , m_RecvQueue{512}
        , m_IntrosetLookupFilter{5s}
    {
      m_state = std::make_unique<EndpointState>();
      m_state->m_Router = r;
      m_state->m_Name = "endpoint";
      m_RecvQueue.enable();

      if (Loop()->MaybeGetUVWLoop())
        m_quic = std::make_unique<quic::TunnelManager>(*this);
    }

    bool
    Endpoint::Configure(const NetworkConfig& conf, [[maybe_unused]] const DnsConfig& dnsConf)
    {
      if (conf.m_Paths.has_value())
        numDesiredPaths = *conf.m_Paths;

      if (conf.m_Hops.has_value())
        numHops = *conf.m_Hops;

      conf.m_ExitMap.ForEachEntry(
          [&](const IPRange& range, const service::Address& addr) { MapExitRange(range, addr); });

      for (auto [exit, auth] : conf.m_ExitAuths)
      {
        SetAuthInfoForEndpoint(exit, auth);
      }

      conf.m_LNSExitMap.ForEachEntry([&](const IPRange& range, const std::string& name) {
        std::optional<AuthInfo> auth;
        const auto itr = conf.m_LNSExitAuths.find(name);
        if (itr != conf.m_LNSExitAuths.end())
          auth = itr->second;
        m_StartupLNSMappings[name] = std::make_pair(range, auth);
      });

      return m_state->Configure(conf);
    }

    bool
    Endpoint::HasPendingPathToService(const Address& addr) const
    {
      return m_state->m_PendingServiceLookups.find(addr) != m_state->m_PendingServiceLookups.end();
    }

    void
    Endpoint::RegenAndPublishIntroSet()
    {
      const auto now = llarp::time_now_ms();
      m_LastIntrosetRegenAttempt = now;
      std::set<Introduction, CompareIntroTimestamp> intros;
      if (const auto maybe =
              GetCurrentIntroductionsWithFilter([now](const service::Introduction& intro) -> bool {
                return not intro.ExpiresSoon(now, path::intro_stale_threshold);
              }))
      {
        intros.insert(maybe->begin(), maybe->end());
      }
      else
      {
        LogWarn(
            "could not publish descriptors for endpoint ",
            Name(),
            " because we couldn't get enough valid introductions");
        BuildOne();
        return;
      }

      introSet().supportedProtocols.clear();

      // add supported ethertypes
      if (HasIfAddr())
      {
        if (IPRange::V4MappedRange().Contains(GetIfAddr()))
        {
          introSet().supportedProtocols.push_back(ProtocolType::TrafficV4);
        }
        else
        {
          introSet().supportedProtocols.push_back(ProtocolType::TrafficV6);
        }

        // exit related stuffo
        if (m_state->m_ExitEnabled)
        {
          introSet().supportedProtocols.push_back(ProtocolType::Exit);
          introSet().exitTrafficPolicy = GetExitPolicy();
          introSet().ownedRanges = GetOwnedRanges();
        }
      }
      // add quic ethertype if we have listeners set up
      if (auto* quic = GetQUICTunnel())
      {
        if (quic->hasListeners())
        {
          log::debug(logcat, "IntroSet setting QUIC as available protocol.");
          introSet().supportedProtocols.push_back(ProtocolType::QUIC);
        }
      }

      introSet().intros.clear();
      for (auto& intro : intros)
      {
        if (introSet().intros.size() < numDesiredPaths)
          introSet().intros.emplace_back(std::move(intro));
      }
      if (introSet().intros.empty())
      {
        LogWarn("not enough intros to publish introset for ", Name());
        if (ShouldBuildMore(now))
          ManualRebuild(1);
        return;
      }
      auto maybe = m_Identity.EncryptAndSignIntroSet(introSet(), now);
      if (not maybe)
      {
        LogWarn("failed to generate introset for endpoint ", Name());
        return;
      }
      if (PublishIntroSet(*maybe, Router()))
      {
        LogInfo("(re)publishing introset for endpoint ", Name());
      }
      else
      {
        LogWarn("failed to publish intro set for endpoint ", Name());
      }
    }

    bool
    Endpoint::IsReady() const
    {
      const auto now = Now();
      if (introSet().intros.empty())
        return false;
      if (introSet().IsExpired(now))
        return false;
      return true;
    }

    bool
    Endpoint::HasPendingRouterLookup(const RouterID remote) const
    {
      const auto& routers = m_state->m_PendingRouters;
      return routers.find(remote) != routers.end();
    }

    std::optional<std::variant<Address, RouterID>>
    Endpoint::GetEndpointWithConvoTag(ConvoTag tag) const
    {
      auto itr = Sessions().find(tag);
      if (itr != Sessions().end())
      {
        return itr->second.remote.Addr();
      }

      for (const auto& item : m_state->m_SNodeSessions)
      {
        if (const auto maybe = item.second->CurrentPath())
        {
          if (ConvoTag{maybe->as_array()} == tag)
            return item.first;
        }
      }
      return std::nullopt;
    }

    void
    Endpoint::map_exit(
        std::string name,
        std::string token,
        std::vector<IPRange> ranges,
        std::function<void(bool, std::string)> result_handler)
    {
      if (ranges.empty())
      {
        result_handler(false, "no ranges provided");
        return;
      }

      LookupNameAsync(
          name,
          [ptr = std::static_pointer_cast<Endpoint>(GetSelf()),
           name,
           auth = AuthInfo{token},
           ranges,
           result_handler,
           poker = m_router->routePoker()](auto maybe_addr) {
            if (not maybe_addr)
            {
              result_handler(false, "exit not found: {}"_format(name));
              return;
            }
            if (auto* addr_ptr = std::get_if<Address>(&*maybe_addr))
            {
              Address addr{*addr_ptr};

              ptr->SetAuthInfoForEndpoint(addr, auth);
              ptr->MarkAddressOutbound(addr);
              auto result = ptr->EnsurePathToService(
                  addr,
                  [ptr, name, ranges, result_handler, poker](auto addr, auto* ctx) {
                    if (ctx == nullptr)
                    {
                      result_handler(false, "could not establish flow to {}"_format(name));
                      return;
                    }

                    // make a lambda that sends the reply after doing auth
                    auto apply_result =
                        [ptr, poker, addr, result_handler, ranges](AuthResult result) {
                          if (result.code != AuthResultCode::eAuthAccepted)
                          {
                            result_handler(false, result.reason);
                            return;
                          }
                          for (const auto& range : ranges)
                            ptr->MapExitRange(range, addr);

                          if (poker)
                            poker->Up();
                          result_handler(true, result.reason);
                        };

                    ctx->AsyncSendAuth(apply_result);
                  },
                  ptr->PathAlignmentTimeout());

              if (not result)
                result_handler(false, "did not build path to {}"_format(name));
            }
            else
              result_handler(false, "exit via snode not supported");
          });
    }

    void
    Endpoint::LookupServiceAsync(
        std::string name,
        std::string service,
        std::function<void(std::vector<dns::SRVData>)> resultHandler)
    {
      // handles when we aligned to a loki address
      auto handleGotPathToService = [resultHandler, service, this](auto addr) {
        // we can probably get this info before we have a path to them but we do this after we
        // have a path so when we send the response back they can send shit to them immediately
        const auto& container = m_state->m_RemoteSessions;
        if (auto itr = container.find(addr); itr != container.end())
        {
          // parse the stuff we need from this guy
          resultHandler(itr->second->GetCurrentIntroSet().GetMatchingSRVRecords(service));
          return;
        }
        resultHandler({});
      };

      // handles when we resolved a .snode
      auto handleResolvedSNodeName = [resultHandler, nodedb = Router()->nodedb()](auto router_id) {
        std::vector<dns::SRVData> result{};
        if (auto maybe_rc = nodedb->Get(router_id))
        {
          result = maybe_rc->srvRecords;
        }
        resultHandler(std::move(result));
      };

      // handles when we got a path to a remote thing
      auto handleGotPathTo = [handleGotPathToService, handleResolvedSNodeName, resultHandler](
                                 auto maybe_tag, auto address) {
        if (not maybe_tag)
        {
          resultHandler({});
          return;
        }

        if (auto* addr = std::get_if<Address>(&address))
        {
          // .loki case
          handleGotPathToService(*addr);
        }
        else if (auto* router_id = std::get_if<RouterID>(&address))
        {
          // .snode case
          handleResolvedSNodeName(*router_id);
        }
        else
        {
          // fallback case
          // XXX: never should happen but we'll handle it anyways
          resultHandler({});
        }
      };

      // handles when we know a long address of a remote resource
      auto handleGotAddress = [resultHandler, handleGotPathTo, this](auto address) {
        // we will attempt a build to whatever we looked up
        const auto result = EnsurePathTo(
            address,
            [address, handleGotPathTo](auto maybe_tag) { handleGotPathTo(maybe_tag, address); },
            PathAlignmentTimeout());

        // on path build start fail short circuit
        if (not result)
          resultHandler({});
      };

      // look up this name async and start the entire chain of events
      LookupNameAsync(name, [handleGotAddress, resultHandler](auto maybe_addr) {
        if (maybe_addr)
        {
          handleGotAddress(*maybe_addr);
        }
        else
        {
          resultHandler({});
        }
      });
    }

    bool
    Endpoint::IntrosetIsStale() const
    {
      return introSet().HasExpiredIntros(Now());
    }

    util::StatusObject
    Endpoint::ExtractStatus() const
    {
      auto obj = path::Builder::ExtractStatus();
      obj["exitMap"] = m_ExitMap.ExtractStatus();
      obj["identity"] = m_Identity.pub.Addr().ToString();
      obj["networkReady"] = ReadyForNetwork();

      util::StatusObject authCodes;
      for (const auto& [service, info] : m_RemoteAuthInfos)
      {
        authCodes[service.ToString()] = info.token;
      }
      obj["authCodes"] = authCodes;

      return m_state->ExtractStatus(obj);
    }

    void
    Endpoint::Tick(llarp_time_t)
    {
      const auto now = llarp::time_now_ms();
      path::Builder::Tick(now);
      // publish descriptors
      if (ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet();
      }
      // decay introset lookup filter
      m_IntrosetLookupFilter.Decay(now);
      // expire name cache
      m_state->nameCache.Decay(now);
      // expire snode sessions
      EndpointUtil::ExpireSNodeSessions(now, m_state->m_SNodeSessions);
      // expire pending tx
      EndpointUtil::ExpirePendingTx(now, m_state->m_PendingLookups);
      // expire pending router lookups
      EndpointUtil::ExpirePendingRouterLookups(now, m_state->m_PendingRouters);

      // deregister dead sessions
      EndpointUtil::DeregisterDeadSessions(now, m_state->m_DeadSessions);
      // tick remote sessions
      EndpointUtil::TickRemoteSessions(
          now, m_state->m_RemoteSessions, m_state->m_DeadSessions, Sessions());
      // expire convotags
      EndpointUtil::ExpireConvoSessions(now, Sessions());

      if (NumInStatus(path::ePathEstablished) > 1)
      {
        for (const auto& item : m_StartupLNSMappings)
        {
          LookupNameAsync(
              item.first, [name = item.first, info = item.second, this](auto maybe_addr) {
                if (maybe_addr.has_value())
                {
                  const auto maybe_range = info.first;
                  const auto maybe_auth = info.second;

                  m_StartupLNSMappings.erase(name);
                  if (auto* addr = std::get_if<service::Address>(&*maybe_addr))
                  {
                    if (maybe_range.has_value())
                      m_ExitMap.Insert(*maybe_range, *addr);
                    if (maybe_auth.has_value())
                      SetAuthInfoForEndpoint(*addr, *maybe_auth);
                  }
                }
              });
        }
      }
    }

    bool
    Endpoint::Stop()
    {
      // stop remote sessions
      log::debug(logcat, "Endpoint stopping remote sessions.");
      EndpointUtil::StopRemoteSessions(m_state->m_RemoteSessions);
      // stop snode sessions
      log::debug(logcat, "Endpoint stopping snode sessions.");
      EndpointUtil::StopSnodeSessions(m_state->m_SNodeSessions);
      log::debug(logcat, "Endpoint stopping its path builder.");
      return path::Builder::Stop();
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid = randint();
      const auto& lookups = m_state->m_PendingLookups;
      while (lookups.find(txid) != lookups.end())
        ++txid;
      return txid;
    }

    std::string
    Endpoint::Name() const
    {
      return m_state->m_Name + ":" + m_Identity.pub.Name();
    }

    void
    Endpoint::PutLookup(IServiceLookup* lookup, uint64_t txid)
    {
      m_state->m_PendingLookups.emplace(txid, std::unique_ptr<IServiceLookup>(lookup));
    }

    bool
    Endpoint::HandleGotIntroMessage(dht::GotIntroMessage_constptr msg)
    {
      std::set<EncryptedIntroSet> remote;
      for (const auto& introset : msg->found)
      {
        if (not introset.Verify(Now()))
        {
          LogError(Name(), " got invalid introset");
          return false;
        }
        remote.insert(introset);
      }
      auto& lookups = m_state->m_PendingLookups;
      auto itr = lookups.find(msg->txid);
      if (itr == lookups.end())
      {
        LogWarn(
            "invalid lookup response for hidden service endpoint ", Name(), " txid=", msg->txid);
        return true;
      }
      std::unique_ptr<IServiceLookup> lookup = std::move(itr->second);
      lookups.erase(itr);
      lookup->HandleIntrosetResponse(remote);
      return true;
    }

    bool
    Endpoint::HasInboundConvo(const Address& addr) const
    {
      for (const auto& item : Sessions())
      {
        if (item.second.remote.Addr() == addr and item.second.inbound)
          return true;
      }
      return false;
    }

    bool
    Endpoint::HasOutboundConvo(const Address& addr) const
    {
      for (const auto& item : Sessions())
      {
        if (item.second.remote.Addr() == addr && not item.second.inbound)
          return true;
      }
      return false;
    }

    void
    Endpoint::PutSenderFor(const ConvoTag& tag, const ServiceInfo& info, bool inbound)
    {
      if (info.Addr().IsZero())
      {
        LogError(Name(), " cannot put invalid service info ", info, " T=", tag);
        return;
      }
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
      {
        if (WantsOutboundSession(info.Addr()) and inbound)
        {
          LogWarn(
              Name(),
              " not adding sender for ",
              info.Addr(),
              " session is inbound and we want outbound T=",
              tag);
          return;
        }
        itr = Sessions().emplace(tag, Session{}).first;
        itr->second.inbound = inbound;
        itr->second.remote = info;
      }
    }

    size_t
    Endpoint::RemoveAllConvoTagsFor(service::Address remote)
    {
      size_t removed = 0;
      auto& sessions = Sessions();
      auto itr = sessions.begin();
      while (itr != sessions.end())
      {
        if (itr->second.remote.Addr() == remote)
        {
          itr = sessions.erase(itr);
          removed++;
        }
        else
          ++itr;
      }
      return removed;
    }

    bool
    Endpoint::GetSenderFor(const ConvoTag& tag, ServiceInfo& si) const
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
        return false;
      si = itr->second.remote;
      si.UpdateAddr();
      return true;
    }

    void
    Endpoint::PutIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto& s = Sessions()[tag];
      s.intro = intro;
    }

    bool
    Endpoint::GetIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
        return false;
      intro = itr->second.intro;
      return true;
    }

    void
    Endpoint::PutReplyIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
      {
        return;
      }
      itr->second.replyIntro = intro;
    }

    bool
    Endpoint::GetReplyIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
        return false;
      intro = itr->second.replyIntro;
      return true;
    }

    bool
    Endpoint::GetConvoTagsForService(const Address& addr, std::set<ConvoTag>& tags) const
    {
      return EndpointUtil::GetConvoTagsForService(Sessions(), addr, tags);
    }

    bool
    Endpoint::GetCachedSessionKeyFor(const ConvoTag& tag, SharedSecret& secret) const
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
        return false;
      secret = itr->second.sharedKey;
      return true;
    }

    void
    Endpoint::PutCachedSessionKeyFor(const ConvoTag& tag, const SharedSecret& k)
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
      {
        itr = Sessions().emplace(tag, Session{}).first;
      }
      itr->second.sharedKey = k;
    }

    void
    Endpoint::ConvoTagTX(const ConvoTag& tag)
    {
      if (Sessions().count(tag))
        Sessions()[tag].TX();
    }

    void
    Endpoint::ConvoTagRX(const ConvoTag& tag)
    {
      if (Sessions().count(tag))
        Sessions()[tag].RX();
    }

    bool
    Endpoint::LoadKeyFile()
    {
      const auto& keyfile = m_state->m_Keyfile;
      if (!keyfile.empty())
      {
        m_Identity.EnsureKeys(keyfile, Router()->keyManager()->needBackup());
      }
      else
      {
        m_Identity.RegenerateKeys();
      }
      return true;
    }

    bool
    Endpoint::Start()
    {
      // how can I tell if a m_Identity isn't loaded?
      if (!m_DataHandler)
      {
        m_DataHandler = this;
      }
      // this does network isolation
      while (m_state->m_OnInit.size())
      {
        if (m_state->m_OnInit.front()())
          m_state->m_OnInit.pop_front();
        else
        {
          LogWarn("Can't call init of network isolation");
          return false;
        }
      }
      return true;
    }

    // Keep this here (rather than the header) so that we don't need to include endpoint_state.hpp
    // in endpoint.hpp for the unique_ptr member destructor.
    Endpoint::~Endpoint() = default;

    bool
    Endpoint::PublishIntroSet(const EncryptedIntroSet& introset, AbstractRouter* r)
    {
      const auto paths = GetManyPathsWithUniqueEndpoints(
          this,
          llarp::dht::IntroSetRelayRedundancy,
          dht::Key_t{introset.derivedSigningKey.as_array()});

      if (paths.size() != llarp::dht::IntroSetRelayRedundancy)
      {
        LogWarn(
            "Cannot publish intro set because we only have ",
            paths.size(),
            " paths, but need ",
            llarp::dht::IntroSetRelayRedundancy);
        return false;
      }

      // do publishing for each path selected
      size_t published = 0;

      for (const auto& path : paths)
      {
        for (size_t i = 0; i < llarp::dht::IntroSetRequestsPerRelay; ++i)
        {
          r->NotifyRouterEvent<tooling::PubIntroSentEvent>(
              r->pubkey(),
              llarp::dht::Key_t{introset.derivedSigningKey.as_array()},
              RouterID(path->hops[path->hops.size() - 1].rc.pubkey),
              published);
          if (PublishIntroSetVia(introset, r, path, published))
            published++;
        }
      }
      if (published != llarp::dht::IntroSetStorageRedundancy)
        LogWarn(
            "Publish introset failed: could only publish ",
            published,
            " copies but wanted ",
            llarp::dht::IntroSetStorageRedundancy);
      return published == llarp::dht::IntroSetStorageRedundancy;
    }

    struct PublishIntroSetJob : public IServiceLookup
    {
      EncryptedIntroSet m_IntroSet;
      Endpoint* m_Endpoint;
      uint64_t m_relayOrder;
      PublishIntroSetJob(
          Endpoint* parent,
          uint64_t id,
          EncryptedIntroSet introset,
          uint64_t relayOrder,
          llarp_time_t timeout)
          : IServiceLookup(parent, id, "PublishIntroSet", timeout)
          , m_IntroSet(std::move(introset))
          , m_Endpoint(parent)
          , m_relayOrder(relayOrder)
      {}

      std::shared_ptr<routing::IMessage>
      BuildRequestMessage() override
      {
        auto msg = std::make_shared<routing::DHTMessage>();
        msg->M.emplace_back(
            std::make_unique<dht::PublishIntroMessage>(m_IntroSet, txid, true, m_relayOrder));
        return msg;
      }

      bool
      HandleIntrosetResponse(const std::set<EncryptedIntroSet>& response) override
      {
        if (not response.empty())
          m_Endpoint->IntroSetPublished();
        else
          m_Endpoint->IntroSetPublishFail();

        return true;
      }
    };

    void
    Endpoint::IntroSetPublishFail()
    {
      auto now = Now();
      if (ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet();
      }
      else if (NumInStatus(path::ePathEstablished) < 3)
      {
        if (introSet().HasExpiredIntros(now))
          ManualRebuild(1);
      }
    }

    size_t
    Endpoint::UniqueEndpoints() const
    {
      return m_state->m_RemoteSessions.size() + m_state->m_SNodeSessions.size();
    }

    constexpr auto PublishIntrosetTimeout = 20s;

    bool
    Endpoint::PublishIntroSetVia(
        const EncryptedIntroSet& introset,
        AbstractRouter* r,
        path::Path_ptr path,
        uint64_t relayOrder)
    {
      auto job =
          new PublishIntroSetJob(this, GenTXID(), introset, relayOrder, PublishIntrosetTimeout);
      if (job->SendRequestViaPath(path, r))
      {
        m_state->m_LastPublishAttempt = Now();
        return true;
      }
      return false;
    }

    void
    Endpoint::ResetInternalState()
    {
      path::Builder::ResetInternalState();
      static auto resetState = [](auto& container, auto getter) {
        std::for_each(container.begin(), container.end(), [getter](auto& item) {
          getter(item)->ResetInternalState();
        });
      };
      resetState(m_state->m_RemoteSessions, [](const auto& item) { return item.second; });
      resetState(m_state->m_SNodeSessions, [](const auto& item) { return item.second; });
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      if (not m_PublishIntroSet)
        return false;

      const auto lastEventAt = std::max(m_state->m_LastPublishAttempt, m_state->m_LastPublish);
      const auto next_pub = lastEventAt
          + (m_state->m_IntroSet.HasStaleIntros(now, path::intro_stale_threshold)
                 ? IntrosetPublishRetryCooldown
                 : IntrosetPublishInterval);

      return now >= next_pub;
    }

    void
    Endpoint::IntroSetPublished()
    {
      const auto now = Now();
      // We usually get 4 confirmations back (one for each DHT location), which
      // is noisy: suppress this log message if we already had a confirmation in
      // the last second.
      if (m_state->m_LastPublish < now - 1s)
        LogInfo(Name(), " IntroSet publish confirmed");
      else
        LogDebug(Name(), " Additional IntroSet publish confirmed");

      m_state->m_LastPublish = now;
    }

    std::optional<std::vector<RouterContact>>
    Endpoint::GetHopsForBuild()
    {
      std::unordered_set<RouterID> exclude;
      ForEachPath([&exclude](auto path) { exclude.insert(path->Endpoint()); });
      const auto maybe =
          m_router->nodedb()->GetRandom([exclude, r = m_router](const auto& rc) -> bool {
            return exclude.count(rc.pubkey) == 0
                and not r->routerProfiling().IsBadForPath(rc.pubkey);
          });
      if (not maybe.has_value())
        return std::nullopt;
      return GetHopsForBuildWithEndpoint(maybe->pubkey);
    }

    std::optional<std::vector<RouterContact>>
    Endpoint::GetHopsForBuildWithEndpoint(RouterID endpoint)
    {
      return path::Builder::GetHopsAlignedToForBuild(endpoint, SnodeBlacklist());
    }

    void
    Endpoint::PathBuildStarted(path::Path_ptr path)
    {
      path::Builder::PathBuildStarted(path);
    }

    constexpr auto MaxOutboundContextPerRemote = 1;

    void
    Endpoint::PutNewOutboundContext(const service::IntroSet& introset, llarp_time_t left)
    {
      const Address addr{introset.addressKeys.Addr()};

      auto& remoteSessions = m_state->m_RemoteSessions;

      if (remoteSessions.count(addr) < MaxOutboundContextPerRemote)
      {
        remoteSessions.emplace(addr, std::make_shared<OutboundContext>(introset, this));
        LogInfo("Created New outbound context for ", addr.ToString());
      }

      auto sessionRange = remoteSessions.equal_range(addr);
      for (auto itr = sessionRange.first; itr != sessionRange.second; ++itr)
      {
        itr->second->AddReadyHook(
            [addr, this](auto session) { InformPathToService(addr, session); }, left);
      }
    }

    void
    Endpoint::HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg, RouterID id, bool valid)
    {
      auto& pendingRouters = m_state->m_PendingRouters;
      auto itr = pendingRouters.find(id);
      if (itr != pendingRouters.end())
      {
        if (valid)
          itr->second.InformResult(msg->foundRCs);
        else
          itr->second.InformResult({});
        pendingRouters.erase(itr);
      }
    }

    bool
    Endpoint::HandleGotRouterMessage(dht::GotRouterMessage_constptr msg)
    {
      if (not msg->foundRCs.empty())
      {
        for (auto& rc : msg->foundRCs)
        {
          Router()->QueueWork([this, rc, msg]() mutable {
            bool valid = rc.Verify(llarp::time_now_ms());
            Router()->loop()->call([this, valid, rc = std::move(rc), msg] {
              Router()->nodedb()->PutIfNewer(rc);
              HandleVerifyGotRouter(msg, rc.pubkey, valid);
            });
          });
        }
      }
      else
      {
        auto& routers = m_state->m_PendingRouters;
        auto itr = routers.begin();
        while (itr != routers.end())
        {
          if (itr->second.txid == msg->txid)
          {
            itr->second.InformResult({});
            itr = routers.erase(itr);
          }
          else
            ++itr;
        }
      }
      return true;
    }

    struct LookupNameJob : public IServiceLookup
    {
      std::function<void(std::optional<Address>)> handler;
      ShortHash namehash;

      LookupNameJob(
          Endpoint* parent,
          uint64_t id,
          std::string lnsName,
          std::function<void(std::optional<Address>)> resultHandler)
          : IServiceLookup(parent, id, lnsName), handler(resultHandler)
      {
        CryptoManager::instance()->shorthash(
            namehash, llarp_buffer_t(lnsName.c_str(), lnsName.size()));
      }

      std::shared_ptr<routing::IMessage>
      BuildRequestMessage() override
      {
        auto msg = std::make_shared<routing::DHTMessage>();
        msg->M.emplace_back(std::make_unique<dht::FindNameMessage>(
            dht::Key_t{}, dht::Key_t{namehash.as_array()}, txid));
        return msg;
      }

      bool
      HandleNameResponse(std::optional<Address> addr) override
      {
        handler(addr);
        return true;
      }

      void
      HandleTimeout() override
      {
        HandleNameResponse(std::nullopt);
      }
    };

    bool
    Endpoint::HasExit() const
    {
      for (const auto& [name, info] : m_StartupLNSMappings)
      {
        if (info.first.has_value())
          return true;
      }

      return not m_ExitMap.Empty();
    }

    path::Path::UniqueEndpointSet_t
    Endpoint::GetUniqueEndpointsForLookup() const
    {
      path::Path::UniqueEndpointSet_t paths;
      ForEachPath([&paths](auto path) {
        if (path and path->IsReady())
          paths.insert(path);
      });
      return paths;
    }

    bool
    Endpoint::ReadyForNetwork() const
    {
      return IsReady() and ReadyToDoLookup(GetUniqueEndpointsForLookup().size());
    }

    bool
    Endpoint::ReadyToDoLookup(size_t num_paths) const
    {
      // Currently just checks the number of paths, but could do more checks in the future.
      return num_paths >= MIN_ENDPOINTS_FOR_LNS_LOOKUP;
    }

    void
    Endpoint::LookupNameAsync(
        std::string name,
        std::function<void(std::optional<std::variant<Address, RouterID>>)> handler)
    {
      if (not NameIsValid(name))
      {
        log::warning(logcat, "\"{}\" is not a valid ONS name", name);
        handler(std::nullopt);
        return;
      }
      auto& cache = m_state->nameCache;
      const auto maybe = cache.Get(name);
      if (maybe.has_value())
      {
        log::debug(logcat, "Returning cached result for ONS name \"{}\"", name);
        handler(maybe);
        return;
      }
      // log::info(logcat, "{} looking up ONS name \"{}\"", Name(), name);
      log::debug(logcat, "{} looking up ONS name \"{}\"", Name(), name);
      auto paths = GetUniqueEndpointsForLookup();
      // not enough paths
      if (not ReadyToDoLookup(paths.size()))
      {
        log::warning(
            logcat,
            "{} not enough paths for ONS lookup, have {} need {}",
            Name(),
            paths.size(),
            MIN_ENDPOINTS_FOR_LNS_LOOKUP);
        handler(std::nullopt);
        return;
      }

      auto maybeInvalidateCache = [handler, &cache, name](auto result) {
        if (result)
        {
          var::visit(
              [&result, &cache, name](auto&& value) {
                if (value.IsZero())
                {
                  cache.Remove(name);
                  result = std::nullopt;
                }
              },
              *result);
        }
        if (result)
        {
          cache.Put(name, *result);
        }
        handler(result);
      };

      constexpr size_t max_lns_lookup_endpoints = 7;
      // pick up to max_unique_lns_endpoints random paths to do lookups from
      std::vector<path::Path_ptr> chosenpaths;
      chosenpaths.insert(chosenpaths.begin(), paths.begin(), paths.end());
      std::shuffle(chosenpaths.begin(), chosenpaths.end(), CSRNG{});
      chosenpaths.resize(std::min(paths.size(), max_lns_lookup_endpoints));

      auto resultHandler =
          m_state->lnsTracker.MakeResultHandler(name, chosenpaths.size(), maybeInvalidateCache);

      for (const auto& path : chosenpaths)
      {
        // log::info(logcat, "{} lookup \"{}\" via {}", Name(), name, path->Endpoint());
        log::debug(logcat, "{} lookup \"{}\" via {}", Name(), name, path->Endpoint());
        auto job = new LookupNameJob{this, GenTXID(), name, resultHandler};
        job->SendRequestViaPath(path, m_router);
      }
    }

    bool
    Endpoint::HandleGotNameMessage(std::shared_ptr<const dht::GotNameMessage> msg)
    {
      auto& lookups = m_state->m_PendingLookups;
      auto itr = lookups.find(msg->TxID);
      if (itr == lookups.end())
        return false;

      // decrypt entry
      const auto maybe = msg->result.Decrypt(itr->second->name);
      // inform result
      itr->second->HandleNameResponse(maybe);
      lookups.erase(itr);
      return true;
    }

    void
    Endpoint::EnsureRouterIsKnown(const RouterID& router)
    {
      if (router.IsZero())
        return;
      if (!Router()->nodedb()->Has(router))
      {
        LookupRouterAnon(router, nullptr);
      }
    }

    bool
    Endpoint::LookupRouterAnon(RouterID router, RouterLookupHandler handler)
    {
      using llarp::dht::FindRouterMessage;

      auto& routers = m_state->m_PendingRouters;
      if (routers.find(router) == routers.end())
      {
        auto path = GetEstablishedPathClosestTo(router);
        routing::DHTMessage msg;
        auto txid = GenTXID();
        msg.M.emplace_back(std::make_unique<FindRouterMessage>(txid, router));
        if (path)
          msg.S = path->NextSeqNo();
        if (path && path->SendRoutingMessage(msg, Router()))
        {
          RouterLookupJob job{this, [handler, router, nodedb = m_router->nodedb()](auto results) {
                                if (results.empty())
                                {
                                  LogInfo("could not find ", router, ", remove it from nodedb");
                                  nodedb->Remove(router);
                                }
                                if (handler)
                                  handler(results);
                              }};

          assert(msg.M.size() == 1);
          auto dhtMsg = dynamic_cast<FindRouterMessage*>(msg.M[0].get());
          assert(dhtMsg != nullptr);

          m_router->NotifyRouterEvent<tooling::FindRouterSentEvent>(m_router->pubkey(), *dhtMsg);

          routers.emplace(router, std::move(job));
          return true;
        }
      }
      return false;
    }

    void
    Endpoint::HandlePathBuilt(path::Path_ptr p)
    {
      p->SetDataHandler(util::memFn(&Endpoint::HandleHiddenServiceFrame, this));
      p->SetDropHandler(util::memFn(&Endpoint::HandleDataDrop, this));
      p->SetDeadChecker(util::memFn(&Endpoint::CheckPathIsDead, this));
      path::Builder::HandlePathBuilt(p);
    }

    bool
    Endpoint::HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t seq)
    {
      LogWarn(Name(), " message ", seq, " dropped by endpoint ", p->Endpoint(), " via ", dst);
      return true;
    }

    std::unordered_map<std::string, std::string>
    Endpoint::NotifyParams() const
    {
      return {{"LOKINET_ADDR", m_Identity.pub.Addr().ToString()}};
    }

    void
    Endpoint::FlushRecvData()
    {
      while (auto maybe = m_RecvQueue.tryPopFront())
      {
        auto& ev = *maybe;
        ProtocolMessage::ProcessAsync(ev.fromPath, ev.pathid, ev.msg);
      }
    }

    void
    Endpoint::QueueRecvData(RecvDataEvent ev)
    {
      m_RecvQueue.tryPushBack(std::move(ev));
      Router()->TriggerPump();
    }

    bool
    Endpoint::HandleDataMessage(
        path::Path_ptr p, const PathID_t from, std::shared_ptr<ProtocolMessage> msg)
    {
      PutSenderFor(msg->tag, msg->sender, true);
      Introduction intro = msg->introReply;
      if (HasInboundConvo(msg->sender.Addr()))
      {
        intro.pathID = from;
        intro.router = p->Endpoint();
      }
      PutReplyIntroFor(msg->tag, intro);
      ConvoTagRX(msg->tag);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HasPathToSNode(const RouterID ident) const
    {
      auto range = m_state->m_SNodeSessions.equal_range(ident);
      auto itr = range.first;
      while (itr != range.second)
      {
        if (itr->second->IsReady())
        {
          return true;
        }
        ++itr;
      }
      return false;
    }

    EndpointBase::AddressVariant_t
    Endpoint::LocalAddress() const
    {
      return m_Identity.pub.Addr();
    }

    std::optional<EndpointBase::SendStat>
    Endpoint::GetStatFor(AddressVariant_t) const
    {
      // TODO: implement me
      return std::nullopt;
    }

    std::unordered_set<EndpointBase::AddressVariant_t>
    Endpoint::AllRemoteEndpoints() const
    {
      std::unordered_set<AddressVariant_t> remote;
      for (const auto& item : Sessions())
      {
        remote.insert(item.second.remote.Addr());
      }
      for (const auto& item : m_state->m_SNodeSessions)
      {
        remote.insert(item.first);
      }
      return remote;
    }

    bool
    Endpoint::ProcessDataMessage(std::shared_ptr<ProtocolMessage> msg)
    {
      if ((msg->proto == ProtocolType::Exit
           && (m_state->m_ExitEnabled || m_ExitMap.ContainsValue(msg->sender.Addr())))
          || msg->proto == ProtocolType::TrafficV4 || msg->proto == ProtocolType::TrafficV6
          || (msg->proto == ProtocolType::QUIC and m_quic))
      {
        m_InboundTrafficQueue.tryPushBack(std::move(msg));
        Router()->TriggerPump();
        return true;
      }
      if (msg->proto == ProtocolType::Control)
      {
        // TODO: implement me (?)
        // right now it's just random noise
        return true;
      }
      return false;
    }

    void
    Endpoint::AsyncProcessAuthMessage(
        std::shared_ptr<ProtocolMessage> msg, std::function<void(AuthResult)> hook)
    {
      if (m_AuthPolicy)
      {
        if (not m_AuthPolicy->AsyncAuthPending(msg->tag))
        {
          // do 1 authentication attempt and drop everything else
          m_AuthPolicy->AuthenticateAsync(std::move(msg), std::move(hook));
        }
      }
      else
      {
        Router()->loop()->call([h = std::move(hook)] { h({AuthResultCode::eAuthAccepted, "OK"}); });
      }
    }

    void
    Endpoint::SendAuthResult(
        path::Path_ptr path, PathID_t replyPath, ConvoTag tag, AuthResult result)
    {
      // not applicable because we are not an exit or don't have an endpoint auth policy
      if ((not m_state->m_ExitEnabled) or m_AuthPolicy == nullptr)
        return;
      ProtocolFrame f{};
      f.R = AuthResultCodeAsInt(result.code);
      f.T = tag;
      f.F = path->intro.pathID;
      f.N.Randomize();
      if (result.code == AuthResultCode::eAuthAccepted)
      {
        ProtocolMessage msg;

        std::vector<byte_t> reason{};
        reason.resize(result.reason.size());
        std::copy_n(result.reason.c_str(), reason.size(), reason.data());
        msg.PutBuffer(reason);
        if (m_AuthPolicy)
          msg.proto = ProtocolType::Auth;
        else
          msg.proto = ProtocolType::Control;

        if (not GetReplyIntroFor(tag, msg.introReply))
        {
          LogError("Failed to send auth reply: no reply intro");
          return;
        }
        msg.sender = m_Identity.pub;
        SharedSecret sessionKey{};
        if (not GetCachedSessionKeyFor(tag, sessionKey))
        {
          LogError("failed to send auth reply: no cached session key");
          return;
        }
        if (not f.EncryptAndSign(msg, sessionKey, m_Identity))
        {
          LogError("Failed to encrypt and sign auth reply");
          return;
        }
      }
      else
      {
        if (not f.Sign(m_Identity))
        {
          LogError("failed to sign auth reply result");
          return;
        }
      }
      m_SendQueue.tryPushBack(
          SendEvent_t{std::make_shared<routing::PathTransferMessage>(f, replyPath), path});
    }

    void
    Endpoint::RemoveConvoTag(const ConvoTag& t)
    {
      Sessions().erase(t);
    }

    void
    Endpoint::ResetConvoTag(ConvoTag tag, path::Path_ptr p, PathID_t from)
    {
      // send reset convo tag message
      ProtocolFrame f{};
      f.R = 1;
      f.T = tag;
      f.F = p->intro.pathID;
      f.Sign(m_Identity);
      {
        LogWarn("invalidating convotag T=", tag);
        RemoveConvoTag(tag);
        m_SendQueue.tryPushBack(
            SendEvent_t{std::make_shared<routing::PathTransferMessage>(f, from), p});
      }
    }

    bool
    Endpoint::HandleHiddenServiceFrame(path::Path_ptr p, const ProtocolFrame& frame)
    {
      if (frame.R)
      {
        // handle discard
        ServiceInfo si;
        if (!GetSenderFor(frame.T, si))
          return false;
        // verify source
        if (!frame.Verify(si))
          return false;
        // remove convotag it doesn't exist
        LogWarn("remove convotag T=", frame.T, " R=", frame.R, " from ", si.Addr());
        RemoveConvoTag(frame.T);
        return true;
      }
      if (not frame.AsyncDecryptAndVerify(Router()->loop(), p, m_Identity, this))
      {
        ResetConvoTag(frame.T, p, frame.F);
      }
      return true;
    }

    void
    Endpoint::HandlePathDied(path::Path_ptr p)
    {
      m_router->routerProfiling().MarkPathTimeout(p.get());
      ManualRebuild(1);
      path::Builder::HandlePathDied(p);
      RegenAndPublishIntroSet();
    }

    bool
    Endpoint::CheckPathIsDead(path::Path_ptr, llarp_time_t dlt)
    {
      return dlt > path::alive_timeout;
    }

    bool
    Endpoint::OnLookup(
        const Address& addr,
        std::optional<IntroSet> introset,
        const RouterID& endpoint,
        llarp_time_t timeLeft,
        uint64_t relayOrder)
    {
      // tell all our existing remote sessions about this introset update

      const auto now = Router()->Now();
      auto& lookups = m_state->m_PendingServiceLookups;
      if (introset)
      {
        auto& sessions = m_state->m_RemoteSessions;
        auto range = sessions.equal_range(addr);
        auto itr = range.first;
        while (itr != range.second)
        {
          itr->second->OnIntroSetUpdate(addr, introset, endpoint, timeLeft, relayOrder);
          // we got a successful lookup
          if (itr->second->ReadyToSend() and not introset->IsExpired(now))
          {
            // inform all lookups
            auto lookup_range = lookups.equal_range(addr);
            auto i = lookup_range.first;
            while (i != lookup_range.second)
            {
              i->second(addr, itr->second.get());
              ++i;
            }
            lookups.erase(addr);
          }
          ++itr;
        }
      }
      auto& fails = m_state->m_ServiceLookupFails;
      if (not introset or introset->IsExpired(now))
      {
        LogError(
            Name(),
            " failed to lookup ",
            addr.ToString(),
            " from ",
            endpoint,
            " order=",
            relayOrder);
        fails[endpoint] = fails[endpoint] + 1;

        const auto pendingForAddr = std::count_if(
            m_state->m_PendingLookups.begin(),
            m_state->m_PendingLookups.end(),
            [addr](const auto& item) -> bool { return item.second->IsFor(addr); });

        // inform all if we have no more pending lookups for this address
        if (pendingForAddr == 0)
        {
          auto range = lookups.equal_range(addr);
          auto itr = range.first;
          while (itr != range.second)
          {
            itr->second(addr, nullptr);
            itr = lookups.erase(itr);
          }
        }
        return false;
      }
      // check for established outbound context

      if (m_state->m_RemoteSessions.count(addr) > 0)
        return true;

      PutNewOutboundContext(*introset, timeLeft);
      return true;
    }

    void
    Endpoint::MarkAddressOutbound(AddressVariant_t addr)
    {
      if (auto* ptr = std::get_if<Address>(&addr))
        m_state->m_OutboundSessions.insert(*ptr);
    }

    bool
    Endpoint::WantsOutboundSession(const Address& addr) const
    {
      return m_state->m_OutboundSessions.count(addr) > 0;
    }

    void
    Endpoint::InformPathToService(const Address remote, OutboundContext* ctx)
    {
      auto& serviceLookups = m_state->m_PendingServiceLookups;
      auto range = serviceLookups.equal_range(remote);
      auto itr = range.first;
      while (itr != range.second)
      {
        itr->second(remote, ctx);
        ++itr;
      }
      serviceLookups.erase(remote);
    }

    bool
    Endpoint::EnsurePathToService(const Address remote, PathEnsureHook hook, llarp_time_t timeout)
    {
      if (not WantsOutboundSession(remote))
      {
        // we don't want to ensure paths to addresses that are inbound
        // inform fail right away in that case
        hook(remote, nullptr);
        return false;
      }

      /// how many routers to use for lookups
      static constexpr size_t NumParallelLookups = 2;
      /// how many requests per router
      static constexpr size_t RequestsPerLookup = 2;

      // add response hook to list for address.
      m_state->m_PendingServiceLookups.emplace(remote, hook);

      auto& sessions = m_state->m_RemoteSessions;
      {
        auto range = sessions.equal_range(remote);
        auto itr = range.first;
        while (itr != range.second)
        {
          if (itr->second->ReadyToSend())
          {
            InformPathToService(remote, itr->second.get());
            return true;
          }
          ++itr;
        }
      }
      /// check replay filter
      if (not m_IntrosetLookupFilter.Insert(remote))
        return true;

      const auto paths = GetManyPathsWithUniqueEndpoints(this, NumParallelLookups);

      using namespace std::placeholders;
      const dht::Key_t location = remote.ToKey();
      uint64_t order = 0;

      // flag to only add callback to list of callbacks for
      // address once.
      bool hookAdded = false;

      for (const auto& path : paths)
      {
        for (size_t count = 0; count < RequestsPerLookup; ++count)
        {
          HiddenServiceAddressLookup* job = new HiddenServiceAddressLookup(
              this,
              [this](auto addr, auto result, auto from, auto left, auto order) {
                return OnLookup(addr, result, from, left, order);
              },
              location,
              PubKey{remote.as_array()},
              path->Endpoint(),
              order,
              GenTXID(),
              timeout + (2 * path->intro.latency) + IntrosetLookupGraceInterval);
          LogInfo(
              "doing lookup for ",
              remote,
              " via ",
              path->Endpoint(),
              " at ",
              location,
              " order=",
              order);
          order++;
          if (job->SendRequestViaPath(path, Router()))
          {
            hookAdded = true;
          }
          else
            LogError(Name(), " send via path failed for lookup");
        }
      }
      return hookAdded;
    }

    void
    Endpoint::SRVRecordsChanged()
    {
      auto& introset = introSet();
      introset.SRVs.clear();
      for (const auto& srv : SRVRecords())
        introset.SRVs.emplace_back(srv.toTuple());

      RegenAndPublishIntroSet();
    }

    bool
    Endpoint::EnsurePathToSNode(const RouterID snode, SNodeEnsureHook h)
    {
      auto& nodeSessions = m_state->m_SNodeSessions;

      using namespace std::placeholders;
      if (nodeSessions.count(snode) == 0)
      {
        const auto src = xhtonl(net::TruncateV6(GetIfAddr()));
        const auto dst = xhtonl(net::TruncateV6(ObtainIPForAddr(snode)));

        auto session = std::make_shared<exit::SNodeSession>(
            snode,
            [=](const llarp_buffer_t& buf) -> bool {
              net::IPPacket pkt;
              if (not pkt.Load(buf))
                return false;
              pkt.UpdateIPv4Address(src, dst);
              /// TODO: V6
              auto itr = m_state->m_SNodeSessions.find(snode);
              if (itr == m_state->m_SNodeSessions.end())
                return false;
              if (const auto maybe = itr->second->CurrentPath())
                return HandleInboundPacket(
                    ConvoTag{maybe->as_array()}, pkt.ConstBuffer(), ProtocolType::TrafficV4, 0);
              return false;
            },
            Router(),
            1,
            numHops,
            false,
            this);
        m_state->m_SNodeSessions[snode] = session;
      }
      EnsureRouterIsKnown(snode);
      auto range = nodeSessions.equal_range(snode);
      auto itr = range.first;
      while (itr != range.second)
      {
        if (itr->second->IsReady())
          h(snode, itr->second, ConvoTag{itr->second->CurrentPath()->as_array()});
        else
        {
          itr->second->AddReadyHook([h, snode](auto session) {
            if (session)
            {
              h(snode, session, ConvoTag{session->CurrentPath()->as_array()});
            }
            else
            {
              h(snode, nullptr, ConvoTag{});
            }
          });
          if (not itr->second->BuildCooldownHit(Now()))
            itr->second->BuildOne();
        }
        ++itr;
      }
      return true;
    }

    bool
    Endpoint::SendToOrQueue(ConvoTag tag, const llarp_buffer_t& pkt, ProtocolType t)
    {
      if (tag.IsZero())
      {
        LogWarn("SendToOrQueue failed: convo tag is zero");
        return false;
      }
      LogDebug(Name(), " send ", pkt.sz, " bytes on T=", tag);
      if (auto maybe = GetEndpointWithConvoTag(tag))
      {
        if (auto* ptr = std::get_if<Address>(&*maybe))
        {
          if (*ptr == m_Identity.pub.Addr())
          {
            ConvoTagTX(tag);
            m_state->m_Router->TriggerPump();
            if (not HandleInboundPacket(tag, pkt, t, 0))
              return false;
            ConvoTagRX(tag);
            return true;
          }
        }
        if (not SendToOrQueue(*maybe, pkt, t))
          return false;
        return true;
      }
      LogDebug("SendToOrQueue failed: no endpoint for convo tag ", tag);
      return false;
    }

    bool
    Endpoint::SendToOrQueue(const RouterID& addr, const llarp_buffer_t& buf, ProtocolType t)
    {
      LogTrace("SendToOrQueue: sending to snode ", addr);
      auto pkt = std::make_shared<net::IPPacket>();
      if (!pkt->Load(buf))
        return false;
      EnsurePathToSNode(
          addr, [this, t, pkt = std::move(pkt)](RouterID, exit::BaseSession_ptr s, ConvoTag) {
            if (s)
            {
              s->SendPacketToRemote(pkt->ConstBuffer(), t);
              Router()->TriggerPump();
            }
          });
      return true;
    }

    void
    Endpoint::Pump(llarp_time_t now)
    {
      FlushRecvData();
      // send downstream packets to user for snode
      for (const auto& [router, session] : m_state->m_SNodeSessions)
        session->FlushDownstream();

      // handle inbound traffic sorted
      util::ascending_priority_queue<ProtocolMessage> queue;
      while (not m_InboundTrafficQueue.empty())
      {
        // succ it out
        queue.emplace(std::move(*m_InboundTrafficQueue.popFront()));
      }
      while (not queue.empty())
      {
        const auto& msg = queue.top();
        LogDebug(
            Name(),
            " handle inbound packet on ",
            msg.tag,
            " ",
            msg.payload.size(),
            " bytes seqno=",
            msg.seqno);
        if (HandleInboundPacket(msg.tag, msg.payload, msg.proto, msg.seqno))
        {
          ConvoTagRX(msg.tag);
        }
        else
        {
          LogWarn("Failed to handle inbound message");
        }
        queue.pop();
      }

      auto router = Router();
      // TODO: locking on this container
      for (const auto& [addr, outctx] : m_state->m_RemoteSessions)
      {
        outctx->FlushUpstream();
        outctx->Pump(now);
      }
      // TODO: locking on this container
      for (const auto& [router, session] : m_state->m_SNodeSessions)
        session->FlushUpstream();

      // send queue flush
      while (not m_SendQueue.empty())
      {
        SendEvent_t item = m_SendQueue.popFront();
        item.first->S = item.second->NextSeqNo();
        if (item.second->SendRoutingMessage(*item.first, router))
          ConvoTagTX(item.first->T.T);
      }

      UpstreamFlush(router);
    }

    std::optional<ConvoTag>
    Endpoint::GetBestConvoTagFor(std::variant<Address, RouterID> remote) const
    {
      // get convotag with lowest estimated RTT
      if (auto ptr = std::get_if<Address>(&remote))
      {
        llarp_time_t rtt = 30s;
        std::optional<ConvoTag> ret = std::nullopt;
        for (const auto& [tag, session] : Sessions())
        {
          if (tag.IsZero())
            continue;
          if (session.remote.Addr() == *ptr)
          {
            if (*ptr == m_Identity.pub.Addr())
            {
              return tag;
            }
            if (session.inbound)
            {
              auto path = GetPathByRouter(session.replyIntro.router);
              // if we have no path to the remote router that's fine still use it just in case this
              // is the ONLY one we have
              if (path == nullptr)
              {
                ret = tag;
                continue;
              }

              if (path and path->IsReady())
              {
                const auto rttEstimate = (session.replyIntro.latency + path->intro.latency) * 2;
                if (rttEstimate < rtt)
                {
                  ret = tag;
                  rtt = rttEstimate;
                }
              }
            }
            else
            {
              auto range = m_state->m_RemoteSessions.equal_range(*ptr);
              auto itr = range.first;
              while (itr != range.second)
              {
                if (itr->second->ReadyToSend() and itr->second->estimatedRTT > 0s)
                {
                  if (itr->second->estimatedRTT < rtt)
                  {
                    ret = tag;
                    rtt = itr->second->estimatedRTT;
                  }
                }
                itr++;
              }
            }
          }
        }
        return ret;
      }
      if (auto* ptr = std::get_if<RouterID>(&remote))
      {
        auto itr = m_state->m_SNodeSessions.find(*ptr);
        if (itr == m_state->m_SNodeSessions.end())
          return std::nullopt;
        if (auto maybe = itr->second->CurrentPath())
          return ConvoTag{maybe->as_array()};
      }
      return std::nullopt;
    }

    bool
    Endpoint::EnsurePathTo(
        std::variant<Address, RouterID> addr,
        std::function<void(std::optional<ConvoTag>)> hook,
        llarp_time_t timeout)
    {
      if (auto ptr = std::get_if<Address>(&addr))
      {
        if (*ptr == m_Identity.pub.Addr())
        {
          ConvoTag tag{};

          if (auto maybe = GetBestConvoTagFor(*ptr))
            tag = *maybe;
          else
            tag.Randomize();
          PutSenderFor(tag, m_Identity.pub, true);
          ConvoTagTX(tag);
          Sessions()[tag].forever = true;
          Loop()->call_soon([tag, hook]() { hook(tag); });
          return true;
        }
        if (not WantsOutboundSession(*ptr))
        {
          // we don't want to connect back to inbound sessions
          hook(std::nullopt);
          return true;
        }

        return EnsurePathToService(
            *ptr,
            [hook](auto, auto* ctx) {
              if (ctx)
              {
                hook(ctx->currentConvoTag);
              }
              else
              {
                hook(std::nullopt);
              }
            },
            timeout);
      }
      if (auto ptr = std::get_if<RouterID>(&addr))
      {
        return EnsurePathToSNode(*ptr, [hook](auto, auto session, auto tag) {
          if (session)
          {
            hook(tag);
          }
          else
          {
            hook(std::nullopt);
          }
        });
      }
      return false;
    }

    bool
    Endpoint::SendToOrQueue(const Address& remote, const llarp_buffer_t& data, ProtocolType t)
    {
      LogTrace("SendToOrQueue: sending to address ", remote);
      if (data.sz == 0)
      {
        LogTrace("SendToOrQueue: dropping because data.sz == 0");
        return false;
      }
      if (HasInboundConvo(remote))
      {
        // inbound conversation
        LogTrace("Have inbound convo");
        auto transfer = std::make_shared<routing::PathTransferMessage>();
        ProtocolFrame& f = transfer->T;
        f.R = 0;
        std::shared_ptr<path::Path> p;
        if (const auto maybe = GetBestConvoTagFor(remote))
        {
          // the remote guy's intro
          Introduction replyIntro;
          SharedSecret K;
          const auto tag = *maybe;

          if (not GetCachedSessionKeyFor(tag, K))
          {
            LogError(Name(), " no cached key for inbound session from ", remote, " T=", tag);
            return false;
          }
          if (not GetReplyIntroFor(tag, replyIntro))
          {
            LogError(Name(), "no reply intro for inbound session from ", remote, " T=", tag);
            return false;
          }
          // get path for intro
          auto p = GetPathByRouter(replyIntro.router);

          if (not p)
          {
            LogWarn(
                Name(),
                " has no path for intro router ",
                RouterID{replyIntro.router},
                " for inbound convo T=",
                tag);
            return false;
          }

          f.T = tag;
          // TODO: check expiration of our end
          auto m = std::make_shared<ProtocolMessage>(f.T);
          m->PutBuffer(data);
          f.N.Randomize();
          f.C.Zero();
          f.R = 0;
          transfer->Y.Randomize();
          m->proto = t;
          m->introReply = p->intro;
          m->sender = m_Identity.pub;
          if (auto maybe = GetSeqNoForConvo(f.T))
          {
            m->seqno = *maybe;
          }
          else
          {
            LogWarn(Name(), " could not set sequence number, no session T=", f.T);
            return false;
          }
          f.S = m->seqno;
          f.F = p->intro.pathID;
          transfer->P = replyIntro.pathID;
          Router()->QueueWork([transfer, p, m, K, this]() {
            if (not transfer->T.EncryptAndSign(*m, K, m_Identity))
            {
              LogError("failed to encrypt and sign for sessionn T=", transfer->T.T);
              return;
            }
            m_SendQueue.tryPushBack(SendEvent_t{transfer, p});
            Router()->TriggerPump();
          });
          return true;
        }
        else
        {
          LogWarn(
              Name(),
              " SendToOrQueue on inbound convo from ",
              remote,
              " but get-best returned none; bug?");
        }
      }
      if (not WantsOutboundSession(remote))
      {
        LogWarn(
            Name(),
            " SendToOrQueue on outbound session we did not mark as outbound (remote=",
            remote,
            ")");
        return false;
      }

      // Failed to find a suitable inbound convo, look for outbound
      LogTrace("Not an inbound convo");
      auto& sessions = m_state->m_RemoteSessions;
      auto range = sessions.equal_range(remote);
      for (auto itr = range.first; itr != range.second; ++itr)
      {
        if (itr->second->ReadyToSend())
        {
          LogTrace("Found an outbound session to use to reach ", remote);
          itr->second->AsyncEncryptAndSendTo(data, t);
          return true;
        }
      }
      LogTrace("Making an outbound session and queuing the data");
      // add pending traffic
      auto& traffic = m_state->m_PendingTraffic;
      traffic[remote].emplace_back(data, t);
      EnsurePathToService(
          remote,
          [this](Address addr, OutboundContext* ctx) {
            if (ctx)
            {
              for (auto& pending : m_state->m_PendingTraffic[addr])
              {
                ctx->AsyncEncryptAndSendTo(pending.Buffer(), pending.protocol);
              }
            }
            else
            {
              LogWarn("no path made to ", addr);
            }
            m_state->m_PendingTraffic.erase(addr);
          },
          PathAlignmentTimeout());
      return true;
    }

    bool
    Endpoint::SendToOrQueue(
        const std::variant<Address, RouterID>& addr, const llarp_buffer_t& data, ProtocolType t)
    {
      return var::visit([&](auto& addr) { return SendToOrQueue(addr, data, t); }, addr);
    }

    bool
    Endpoint::HasConvoTag(const ConvoTag& t) const
    {
      return Sessions().find(t) != Sessions().end();
    }

    std::optional<uint64_t>
    Endpoint::GetSeqNoForConvo(const ConvoTag& tag)
    {
      auto itr = Sessions().find(tag);
      if (itr == Sessions().end())
        return std::nullopt;
      return itr->second.seqno++;
    }

    bool
    Endpoint::ShouldBuildMore(llarp_time_t now) const
    {
      if (BuildCooldownHit(now))
        return false;
      const auto requiredPaths = std::max(numDesiredPaths, path::min_intro_paths);
      if (NumInStatus(path::ePathBuilding) >= requiredPaths)
        return false;
      return NumPathsExistingAt(now + (path::default_lifetime - path::intro_path_spread))
          < requiredPaths;
    }

    AbstractRouter*
    Endpoint::Router()
    {
      return m_state->m_Router;
    }

    const EventLoop_ptr&
    Endpoint::Loop()
    {
      return Router()->loop();
    }

    void
    Endpoint::BlacklistSNode(const RouterID snode)
    {
      m_state->m_SnodeBlacklist.insert(snode);
    }

    const std::set<RouterID>&
    Endpoint::SnodeBlacklist() const
    {
      return m_state->m_SnodeBlacklist;
    }

    const IntroSet&
    Endpoint::introSet() const
    {
      return m_state->m_IntroSet;
    }

    IntroSet&
    Endpoint::introSet()
    {
      return m_state->m_IntroSet;
    }

    const ConvoMap&
    Endpoint::Sessions() const
    {
      return m_state->m_Sessions;
    }

    ConvoMap&
    Endpoint::Sessions()
    {
      return m_state->m_Sessions;
    }

    void
    Endpoint::SetAuthInfoForEndpoint(Address addr, AuthInfo info)
    {
      if (info.token.empty())
      {
        m_RemoteAuthInfos.erase(addr);
        return;
      }
      m_RemoteAuthInfos[addr] = std::move(info);
    }

    void
    Endpoint::MapExitRange(IPRange range, Address exit)
    {
      if (not exit.IsZero())
        LogInfo(Name(), " map ", range, " to exit at ", exit);
      m_ExitMap.Insert(range, exit);
    }
    bool
    Endpoint::HasFlowToService(Address addr) const
    {
      return HasOutboundConvo(addr) or HasInboundConvo(addr);
    }

    void
    Endpoint::UnmapExitRange(IPRange range)
    {
      // unmap all ranges that fit in the range we gave
      m_ExitMap.RemoveIf([&](const auto& item) -> bool {
        if (not range.Contains(item.first))
          return false;
        LogInfo(Name(), " unmap ", item.first, " exit range mapping");
        return true;
      });

      if (m_ExitMap.Empty())
        m_router->routePoker()->Down();
    }

    void
    Endpoint::UnmapRangeByExit(IPRange range, std::string exit)
    {
      // unmap all ranges that match the given exit when hot swapping
      m_ExitMap.RemoveIf([&](const auto& item) -> bool {
        if ((range.Contains(item.first)) and (item.second.ToString() == exit))
        {
          log::info(logcat, "{} unmap {} range mapping to exit node {}", Name(), item.first, exit);
          return true;
        }
        return false;
      });

      if (m_ExitMap.Empty())
        m_router->routePoker()->Down();
    }

    std::optional<AuthInfo>
    Endpoint::MaybeGetAuthInfoForEndpoint(Address remote)
    {
      const auto itr = m_RemoteAuthInfos.find(remote);
      if (itr == m_RemoteAuthInfos.end())
        return std::nullopt;
      return itr->second;
    }

    quic::TunnelManager*
    Endpoint::GetQUICTunnel()
    {
      return m_quic.get();
    }

  }  // namespace service
}  // namespace llarp

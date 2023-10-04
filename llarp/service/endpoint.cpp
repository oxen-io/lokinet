#include "endpoint.hpp"
#include "endpoint_state.hpp"
#include "endpoint_util.hpp"
#include "auth.hpp"
#include "llarp/util/logging.hpp"
#include "outbound_context.hpp"
#include "protocol.hpp"
#include "info.hpp"
#include "protocol_type.hpp"

#include <llarp/net/ip.hpp>
#include <llarp/net/ip_range.hpp>
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
#include <llarp/router/router.hpp>
#include <llarp/router/route_poker.hpp>
#include <llarp/routing/path_dht_message.hpp>
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

    Endpoint::Endpoint(Router* r, Context* parent)
        : path::Builder{r, 3, path::DEFAULT_LEN}
        , context{parent}
        , _inbound_queue{512}
        , _send_queue{512}
        , _recv_event_queue{512}
        , _introset_lookup_filter{5s}
    {
      _state = std::make_unique<EndpointState>();
      _state->router = r;
      _state->name = "endpoint";
      _recv_event_queue.enable();

      if (Loop()->MaybeGetUVWLoop())
        _tunnel_manager = std::make_unique<quic::TunnelManager>(*this);
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
        _startup_ons_mappings[name] = std::make_pair(range, auth);
      });

      return _state->Configure(conf);
    }

    bool
    Endpoint::HasPendingPathToService(const Address& addr) const
    {
      return _state->pending_service_lookups.find(addr) != _state->pending_service_lookups.end();
    }

    bool
    Endpoint::is_ready() const
    {
      const auto now = llarp::time_now_ms();
      if (intro_set().intros.empty())
        return false;
      if (intro_set().IsExpired(now))
        return false;
      return true;
    }

    bool
    Endpoint::HasPendingRouterLookup(const RouterID remote) const
    {
      const auto& routers = _state->pending_routers;
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

      for (const auto& item : _state->snode_sessions)
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
           poker = router()->route_poker()](auto maybe_addr) {
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
                            poker->put_up();
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
        const auto& container = _state->remote_sessions;
        if (auto itr = container.find(addr); itr != container.end())
        {
          // parse the stuff we need from this guy
          resultHandler(itr->second->GetCurrentIntroSet().GetMatchingSRVRecords(service));
          return;
        }
        resultHandler({});
      };

      // handles when we resolved a .snode
      auto handleResolvedSNodeName = [resultHandler, nodedb = router()->node_db()](auto router_id) {
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
      return intro_set().HasExpiredIntros(llarp::time_now_ms());
    }

    util::StatusObject
    Endpoint::ExtractStatus() const
    {
      auto obj = path::Builder::ExtractStatus();
      obj["exitMap"] = _exit_map.ExtractStatus();
      obj["identity"] = _identity.pub.Addr().ToString();
      obj["networkReady"] = ReadyForNetwork();

      util::StatusObject authCodes;
      for (const auto& [service, info] : _remote_auth_infos)
      {
        authCodes[service.ToString()] = info.token;
      }
      obj["authCodes"] = authCodes;

      return _state->ExtractStatus(obj);
    }

    void
    Endpoint::Tick(llarp_time_t)
    {
      const auto now = llarp::time_now_ms();
      path::Builder::Tick(now);
      // publish descriptors
      if (ShouldPublishDescriptors(now))
      {
        regen_and_publish_introset();
      }
      // decay introset lookup filter
      _introset_lookup_filter.Decay(now);
      // expire name cache
      _state->nameCache.Decay(now);
      // expire snode sessions
      EndpointUtil::ExpireSNodeSessions(now, _state->snode_sessions);
      // expire pending router lookups
      EndpointUtil::ExpirePendingRouterLookups(now, _state->pending_routers);

      // deregister dead sessions
      EndpointUtil::DeregisterDeadSessions(now, _state->dead_sessions);
      // tick remote sessions
      EndpointUtil::TickRemoteSessions(
          now, _state->remote_sessions, _state->dead_sessions, Sessions());
      // expire convotags
      EndpointUtil::ExpireConvoSessions(now, Sessions());

      if (NumInStatus(path::ePathEstablished) > 1)
      {
        for (const auto& item : _startup_ons_mappings)
        {
          LookupNameAsync(
              item.first, [name = item.first, info = item.second, this](auto maybe_addr) {
                if (maybe_addr.has_value())
                {
                  const auto maybe_range = info.first;
                  const auto maybe_auth = info.second;

                  _startup_ons_mappings.erase(name);
                  if (auto* addr = std::get_if<service::Address>(&*maybe_addr))
                  {
                    if (maybe_range.has_value())
                      _exit_map.Insert(*maybe_range, *addr);
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
      EndpointUtil::StopRemoteSessions(_state->remote_sessions);
      // stop snode sessions
      log::debug(logcat, "Endpoint stopping snode sessions.");
      EndpointUtil::StopSnodeSessions(_state->snode_sessions);
      log::debug(logcat, "Endpoint stopping its path builder.");
      return path::Builder::Stop();
    }

    uint64_t
    Endpoint::GenTXID()
    {
      return randint();
    }

    std::string
    Endpoint::Name() const
    {
      return _state->name + ":" + _identity.pub.Name();
    }

    bool
    Endpoint::HandleGotIntroMessage(dht::GotIntroMessage_constptr msg)
    {
      std::set<EncryptedIntroSet> remote;
      for (const auto& introset : msg->found)
      {
        if (not introset.verify(Now()))
        {
          LogError(Name(), " got invalid introset");
          return false;
        }
        remote.insert(introset);
      }
      auto& lookups = _state->pending_lookups;
      auto itr = lookups.find(msg->txid);
      if (itr == lookups.end())
      {
        LogWarn(
            "invalid lookup response for hidden service endpoint ", Name(), " txid=", msg->txid);
        return true;
      }
      std::unique_ptr<IServiceLookup> lookup = std::move(itr->second);
      lookups.erase(itr);
      // lookup->HandleIntrosetResponse(remote);
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
      const auto& keyfile = _state->key_file;
      if (!keyfile.empty())
      {
        _identity.EnsureKeys(keyfile, router()->key_manager()->needBackup());
      }
      else
      {
        _identity.RegenerateKeys();
      }
      return true;
    }

    bool
    Endpoint::Start()
    {
      // this does network isolation
      while (_state->on_init_callbacks.size())
      {
        if (_state->on_init_callbacks.front()())
          _state->on_init_callbacks.pop_front();
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

    void
    Endpoint::regen_and_publish_introset()
    {
      const auto now = llarp::time_now_ms();
      _last_introset_regen_attempt = now;
      std::set<Introduction, CompareIntroTimestamp> intros;

      if (const auto maybe =
              GetCurrentIntroductionsWithFilter([now](const service::Introduction& intro) -> bool {
                return not intro.ExpiresSoon(now, path::INTRO_STALE_THRESHOLD);
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

      intro_set().supported_protocols.clear();

      // add supported ethertypes
      if (HasIfAddr())
      {
        if (IPRange::V4MappedRange().Contains(GetIfAddr()))
        {
          intro_set().supported_protocols.push_back(ProtocolType::TrafficV4);
        }
        else
        {
          intro_set().supported_protocols.push_back(ProtocolType::TrafficV6);
        }

        // exit related stuffo
        if (_state->is_exit_enabled)
        {
          intro_set().supported_protocols.push_back(ProtocolType::Exit);
          intro_set().exit_policy = GetExitPolicy();
          intro_set().owned_ranges = GetOwnedRanges();
        }
      }
      // add quic ethertype if we have listeners set up
      if (auto* quic = GetQUICTunnel())
      {
        if (quic->hasListeners())
          intro_set().supported_protocols.push_back(ProtocolType::QUIC);
      }

      intro_set().intros.clear();
      for (auto& intro : intros)
      {
        if (intro_set().intros.size() < numDesiredPaths)
          intro_set().intros.emplace_back(std::move(intro));
      }
      if (intro_set().intros.empty())
      {
        LogWarn("not enough intros to publish introset for ", Name());
        if (ShouldBuildMore(now))
          ManualRebuild(1);
        return;
      }
      auto maybe = _identity.encrypt_and_sign_introset(intro_set(), now);
      if (not maybe)
      {
        LogWarn("failed to generate introset for endpoint ", Name());
        return;
      }
      if (publish_introset(*maybe))
      {
        LogInfo("(re)publishing introset for endpoint ", Name());
      }
      else
      {
        LogWarn("failed to publish intro set for endpoint ", Name());
      }
    }

    bool
    Endpoint::publish_introset(const EncryptedIntroSet& introset)
    {
      const auto paths = GetManyPathsWithUniqueEndpoints(
          this, INTROSET_RELAY_REDUNDANCY, dht::Key_t{introset.derivedSigningKey.as_array()});

      if (paths.size() != INTROSET_RELAY_REDUNDANCY)
      {
        LogWarn(
            "Cannot publish intro set because we only have ",
            paths.size(),
            " paths, but need ",
            INTROSET_RELAY_REDUNDANCY);
        return false;
      }

      for (const auto& path : paths)
      {
        for (size_t i = 0; i < INTROSET_REQS_PER_RELAY; ++i)
        {
          router()->send_control_message(path->upstream(), "publish_intro", introset.bt_encode());
        }
      }

      return true;
    }

    size_t
    Endpoint::UniqueEndpoints() const
    {
      return _state->remote_sessions.size() + _state->snode_sessions.size();
    }

    constexpr auto PublishIntrosetTimeout = 20s;

    void
    Endpoint::ResetInternalState()
    {
      path::Builder::ResetInternalState();
      static auto resetState = [](auto& container, auto getter) {
        std::for_each(container.begin(), container.end(), [getter](auto& item) {
          getter(item)->ResetInternalState();
        });
      };
      resetState(_state->remote_sessions, [](const auto& item) { return item.second; });
      resetState(_state->snode_sessions, [](const auto& item) { return item.second; });
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      if (not _publish_introset)
        return false;

      const auto lastEventAt = std::max(_state->last_publish_attempt, _state->last_publish);
      const auto next_pub = lastEventAt
          + (_state->local_introset.HasStaleIntros(now, path::INTRO_STALE_THRESHOLD)
                 ? IntrosetPublishRetryCooldown
                 : IntrosetPublishInterval);

      return now >= next_pub;
    }

    std::optional<std::vector<RouterContact>>
    Endpoint::GetHopsForBuild()
    {
      std::unordered_set<RouterID> exclude;
      ForEachPath([&exclude](auto path) { exclude.insert(path->Endpoint()); });
      const auto maybe =
          router()->node_db()->GetRandom([exclude, r = router()](const auto& rc) -> bool {
            return exclude.count(rc.pubkey) == 0
                and not r->router_profiling().IsBadForPath(rc.pubkey);
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
      const Address addr{introset.address_keys.Addr()};

      auto& remoteSessions = _state->remote_sessions;

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
      auto& pendingRouters = _state->pending_routers;
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
          router()->queue_work([this, rc, msg]() mutable {
            bool valid = rc.Verify(llarp::time_now_ms());
            router()->loop()->call([this, valid, rc = std::move(rc), msg] {
              router()->node_db()->PutIfNewer(rc);
              HandleVerifyGotRouter(msg, rc.pubkey, valid);
            });
          });
        }
      }
      else
      {
        auto& routers = _state->pending_routers;
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

    bool
    Endpoint::HasExit() const
    {
      for (const auto& [name, info] : _startup_ons_mappings)
      {
        if (info.first.has_value())
          return true;
      }

      return not _exit_map.Empty();
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
      return is_ready() and ReadyToDoLookup(GetUniqueEndpointsForLookup().size());
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
        handler(ParseAddress(name));
        return;
      }
      auto& cache = _state->nameCache;
      const auto maybe = cache.Get(name);
      if (maybe.has_value())
      {
        handler(maybe);
        return;
      }
      LogInfo(Name(), " looking up LNS name: ", name);
      auto paths = GetUniqueEndpointsForLookup();
      // not enough paths
      if (not ReadyToDoLookup(paths.size()))
      {
        LogWarn(
            Name(),
            " not enough paths for lns lookup, have ",
            paths.size(),
            " need ",
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
          _state->lnsTracker.MakeResultHandler(name, chosenpaths.size(), maybeInvalidateCache);

      for (const auto& path : chosenpaths)
      {
        LogInfo(Name(), " lookup ", name, " from ", path->Endpoint());
        auto job = new LookupNameJob{this, GenTXID(), name, resultHandler};
        job->SendRequestViaPath(path, router);
      }
    }

    bool
    Endpoint::HandleGotNameMessage(std::shared_ptr<const dht::GotNameMessage> msg)
    {
      auto& lookups = _state->pending_lookups;
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
    Endpoint::EnsureRouterIsKnown(const RouterID& rid)
    {
      if (rid.IsZero())
        return;
      if (!router()->node_db()->Has(rid))
      {
        LookupRouterAnon(rid, nullptr);
      }
    }

    bool
    Endpoint::LookupRouterAnon(RouterID rid, RouterLookupHandler handler)
    {
      using llarp::dht::FindRouterMessage;

      auto& routers = _state->pending_routers;
      if (routers.find(rid) == routers.end())
      {
        auto path = GetEstablishedPathClosestTo(rid);
        routing::PathDHTMessage msg;
        auto txid = GenTXID();
        msg.dht_msgs.emplace_back(std::make_unique<FindRouterMessage>(txid, rid));
        if (path)
          msg.sequence_number = path->NextSeqNo();
        if (path && path->SendRoutingMessage(msg, router()))
        {
          RouterLookupJob job{this, [handler, rid, nodedb = router()->node_db()](auto results) {
                                if (results.empty())
                                {
                                  LogInfo("could not find ", rid, ", remove it from nodedb");
                                  nodedb->Remove(rid);
                                }
                                if (handler)
                                  handler(results);
                              }};

          assert(msg.dht_msgs.size() == 1);
          auto dhtMsg = dynamic_cast<FindRouterMessage*>(msg.dht_msgs[0].get());
          assert(dhtMsg != nullptr);

          routers.emplace(rid, std::move(job));
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
      return {{"LOKINET_ADDR", _identity.pub.Addr().ToString()}};
    }

    void
    Endpoint::FlushRecvData()
    {
      while (auto maybe = _recv_event_queue.tryPopFront())
      {
        auto& ev = *maybe;
        ProtocolMessage::ProcessAsync(ev.fromPath, ev.pathid, ev.msg);
      }
    }

    void
    Endpoint::QueueRecvData(RecvDataEvent ev)
    {
      _recv_event_queue.tryPushBack(std::move(ev));
      router()->TriggerPump();
    }

    bool
    Endpoint::HandleDataMessage(
        path::Path_ptr p, const PathID_t from, std::shared_ptr<ProtocolMessage> msg)
    {
      PutSenderFor(msg->tag, msg->sender, true);
      Introduction intro = msg->introReply;
      if (HasInboundConvo(msg->sender.Addr()))
      {
        intro.path_id = from;
        intro.router = p->Endpoint();
      }
      PutReplyIntroFor(msg->tag, intro);
      ConvoTagRX(msg->tag);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HasPathToSNode(const RouterID ident) const
    {
      auto range = _state->snode_sessions.equal_range(ident);
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
      return _identity.pub.Addr();
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
      for (const auto& item : _state->snode_sessions)
      {
        remote.insert(item.first);
      }
      return remote;
    }

    bool
    Endpoint::ProcessDataMessage(std::shared_ptr<ProtocolMessage> msg)
    {
      if ((msg->proto == ProtocolType::Exit
           && (_state->is_exit_enabled || _exit_map.ContainsValue(msg->sender.Addr())))
          || msg->proto == ProtocolType::TrafficV4 || msg->proto == ProtocolType::TrafficV6
          || (msg->proto == ProtocolType::QUIC and _tunnel_manager))
      {
        _inbound_queue.tryPushBack(std::move(msg));
        router()->TriggerPump();
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
      if (_auth_policy)
      {
        if (not _auth_policy->AsyncAuthPending(msg->tag))
        {
          // do 1 authentication attempt and drop everything else
          _auth_policy->AuthenticateAsync(std::move(msg), std::move(hook));
        }
      }
      else
      {
        router()->loop()->call([h = std::move(hook)] { h({AuthResultCode::eAuthAccepted, "OK"}); });
      }
    }

    void
    Endpoint::SendAuthResult(
        path::Path_ptr path, PathID_t replyPath, ConvoTag tag, AuthResult result)
    {
      // not applicable because we are not an exit or don't have an endpoint auth policy
      if ((not _state->is_exit_enabled) or _auth_policy == nullptr)
        return;
      ProtocolFrameMessage f{};
      f.flag = AuthResultCodeAsInt(result.code);
      f.convo_tag = tag;
      f.path_id = path->intro.path_id;
      f.nonce.Randomize();
      if (result.code == AuthResultCode::eAuthAccepted)
      {
        ProtocolMessage msg;

        std::vector<byte_t> reason{};
        reason.resize(result.reason.size());
        std::copy_n(result.reason.c_str(), reason.size(), reason.data());
        msg.PutBuffer(reason);
        if (_auth_policy)
          msg.proto = ProtocolType::Auth;
        else
          msg.proto = ProtocolType::Control;

        if (not GetReplyIntroFor(tag, msg.introReply))
        {
          LogError("Failed to send auth reply: no reply intro");
          return;
        }
        msg.sender = _identity.pub;
        SharedSecret sessionKey{};
        if (not GetCachedSessionKeyFor(tag, sessionKey))
        {
          LogError("failed to send auth reply: no cached session key");
          return;
        }
        if (not f.EncryptAndSign(msg, sessionKey, _identity))
        {
          LogError("Failed to encrypt and sign auth reply");
          return;
        }
      }
      else
      {
        if (not f.Sign(_identity))
        {
          LogError("failed to sign auth reply result");
          return;
        }
      }
      _send_queue.tryPushBack(
          SendEvent{std::make_shared<routing::PathTransferMessage>(f, replyPath), path});
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
      ProtocolFrameMessage f{};
      f.flag = 1;
      f.convo_tag = tag;
      f.path_id = p->intro.path_id;
      f.Sign(_identity);
      {
        LogWarn("invalidating convotag T=", tag);
        RemoveConvoTag(tag);
        _send_queue.tryPushBack(
            SendEvent{std::make_shared<routing::PathTransferMessage>(f, from), p});
      }
    }

    bool
    Endpoint::HandleHiddenServiceFrame(path::Path_ptr p, const ProtocolFrameMessage& frame)
    {
      if (frame.flag)
      {
        // handle discard
        ServiceInfo si;
        if (!GetSenderFor(frame.convo_tag, si))
          return false;
        // verify source
        if (!frame.Verify(si))
          return false;
        // remove convotag it doesn't exist
        LogWarn("remove convotag T=", frame.convo_tag, " R=", frame.flag, " from ", si.Addr());
        RemoveConvoTag(frame.convo_tag);
        return true;
      }
      if (not frame.AsyncDecryptAndVerify(router()->loop(), p, _identity, this))
      {
        ResetConvoTag(frame.convo_tag, p, frame.path_id);
      }
      return true;
    }

    void
    Endpoint::HandlePathDied(path::Path_ptr p)
    {
      router()->router_profiling().MarkPathTimeout(p.get());
      ManualRebuild(1);
      path::Builder::HandlePathDied(p);
      regen_and_publish_introset();
    }

    bool
    Endpoint::CheckPathIsDead(path::Path_ptr, llarp_time_t dlt)
    {
      return dlt > path::ALIVE_TIMEOUT;
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

      const auto now = router()->now();
      auto& lookups = _state->pending_service_lookups;
      if (introset)
      {
        auto& sessions = _state->remote_sessions;
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
      auto& fails = _state->service_lookup_fails;
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
            _state->pending_lookups.begin(),
            _state->pending_lookups.end(),
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

      if (_state->remote_sessions.count(addr) > 0)
        return true;

      PutNewOutboundContext(*introset, timeLeft);
      return true;
    }

    void
    Endpoint::MarkAddressOutbound(AddressVariant_t addr)
    {
      if (auto* ptr = std::get_if<Address>(&addr))
        _state->m_OutboundSessions.insert(*ptr);
    }

    bool
    Endpoint::WantsOutboundSession(const Address& addr) const
    {
      return _state->m_OutboundSessions.count(addr) > 0;
    }

    void
    Endpoint::InformPathToService(const Address remote, OutboundContext* ctx)
    {
      auto& serviceLookups = _state->pending_service_lookups;
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
    Endpoint::EnsurePathToService(
        const Address remote, PathEnsureHook hook, [[maybe_unused]] llarp_time_t timeout)
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
      _state->pending_service_lookups.emplace(remote, hook);

      auto& sessions = _state->remote_sessions;
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
      if (not _introset_lookup_filter.Insert(remote))
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
          if (job->SendRequestViaPath(path, router()))
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
      auto& introset = intro_set();
      introset.SRVs.clear();
      for (const auto& srv : SRVRecords())
        introset.SRVs.emplace_back(srv.toTuple());

      regen_and_publish_introset();
    }

    bool
    Endpoint::EnsurePathToSNode(const RouterID snode, SNodeEnsureHook h)
    {
      auto& nodeSessions = _state->snode_sessions;

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
              auto itr = _state->snode_sessions.find(snode);
              if (itr == _state->snode_sessions.end())
                return false;
              if (const auto maybe = itr->second->CurrentPath())
                return HandleInboundPacket(
                    ConvoTag{maybe->as_array()}, pkt.ConstBuffer(), ProtocolType::TrafficV4, 0);
              return false;
            },
            router(),
            1,
            numHops,
            false,
            this);
        _state->snode_sessions[snode] = session;
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
          if (*ptr == _identity.pub.Addr())
          {
            ConvoTagTX(tag);
            _state->router->TriggerPump();
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

    void
    Endpoint::Pump(llarp_time_t now)
    {
      FlushRecvData();
      // send downstream packets to user for snode
      for (const auto& [router, session] : _state->snode_sessions)
        session->FlushDownstream();

      // handle inbound traffic sorted
      util::ascending_priority_queue<ProtocolMessage> queue;
      while (not _inbound_queue.empty())
      {
        // succ it out
        queue.emplace(std::move(*_inbound_queue.popFront()));
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

      auto r = router();
      // TODO: locking on this container
      for (const auto& [addr, outctx] : _state->remote_sessions)
      {
        outctx->FlushUpstream();
        outctx->Pump(now);
      }
      // TODO: locking on this container
      for (const auto& [r, session] : _state->snode_sessions)
        session->FlushUpstream();

      // send queue flush
      while (not _send_queue.empty())
      {
        SendEvent item = _send_queue.popFront();
        item.first->sequence_number = item.second->NextSeqNo();
        if (item.second->SendRoutingMessage(*item.first, r))
          ConvoTagTX(item.first->protocol_frame_msg.convo_tag);
      }

      UpstreamFlush(r);
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
            if (*ptr == _identity.pub.Addr())
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
              auto range = _state->remote_sessions.equal_range(*ptr);
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
        auto itr = _state->snode_sessions.find(*ptr);
        if (itr == _state->snode_sessions.end())
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
        if (*ptr == _identity.pub.Addr())
        {
          ConvoTag tag{};

          if (auto maybe = GetBestConvoTagFor(*ptr))
            tag = *maybe;
          else
            tag.Randomize();
          PutSenderFor(tag, _identity.pub, true);
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
      const auto requiredPaths = std::max(numDesiredPaths, path::MIN_INTRO_PATHS);
      if (NumInStatus(path::ePathBuilding) >= requiredPaths)
        return false;
      return NumPathsExistingAt(now + (path::DEFAULT_LIFETIME - path::INTRO_PATH_SPREAD))
          < requiredPaths;
    }

    Router*
    Endpoint::router()
    {
      return _state->router;
    }

    const EventLoop_ptr&
    Endpoint::Loop()
    {
      return router()->loop();
    }

    void
    Endpoint::BlacklistSNode(const RouterID snode)
    {
      _state->snode_blacklist.insert(snode);
    }

    const std::set<RouterID>&
    Endpoint::SnodeBlacklist() const
    {
      return _state->snode_blacklist;
    }

    const IntroSet&
    Endpoint::intro_set() const
    {
      return _state->local_introset;
    }

    IntroSet&
    Endpoint::intro_set()
    {
      return _state->local_introset;
    }

    const std::unordered_map<ConvoTag, Session>&
    Endpoint::Sessions() const
    {
      return _state->m_Sessions;
    }

    std::unordered_map<ConvoTag, Session>&
    Endpoint::Sessions()
    {
      return _state->m_Sessions;
    }

    void
    Endpoint::SetAuthInfoForEndpoint(Address addr, AuthInfo info)
    {
      if (info.token.empty())
      {
        _remote_auth_infos.erase(addr);
        return;
      }
      _remote_auth_infos[addr] = std::move(info);
    }

    void
    Endpoint::MapExitRange(IPRange range, Address exit)
    {
      if (not exit.IsZero())
        LogInfo(Name(), " map ", range, " to exit at ", exit);
      _exit_map.Insert(range, exit);
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
      _exit_map.RemoveIf([&](const auto& item) -> bool {
        if (not range.Contains(item.first))
          return false;
        LogInfo(Name(), " unmap ", item.first, " exit range mapping");
        return true;
      });

      if (_exit_map.Empty())
        router()->route_poker()->put_down();
    }

    void
    Endpoint::UnmapRangeByExit(IPRange range, std::string exit)
    {
      // unmap all ranges that match the given exit when hot swapping
      _exit_map.RemoveIf([&](const auto& item) -> bool {
        if ((range.Contains(item.first)) and (item.second.ToString() == exit))
        {
          log::info(logcat, "{} unmap {} range mapping to exit node {}", Name(), item.first, exit);
          return true;
        }
        return false;
      });

      if (_exit_map.Empty())
        router()->route_poker()->put_down();
    }

    std::optional<AuthInfo>
    Endpoint::MaybeGetAuthInfoForEndpoint(Address remote)
    {
      const auto itr = _remote_auth_infos.find(remote);
      if (itr == _remote_auth_infos.end())
        return std::nullopt;
      return itr->second;
    }

    quic::TunnelManager*
    Endpoint::GetQUICTunnel()
    {
      return _tunnel_manager.get();
    }

  }  // namespace service
}  // namespace llarp

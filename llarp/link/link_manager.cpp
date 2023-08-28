#include "link_manager.hpp"

#include <llarp/router/i_rc_lookup_handler.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/crypto/crypto.hpp>

#include <algorithm>
#include <set>

namespace llarp
{
  link::Endpoint*
  LinkManager::GetCompatibleLink(const RouterContact& rc)
  {
    if (stopping)
      return nullptr;

    for (auto& ep : endpoints)
    {
      //TODO: need some notion of "is this link compatible with that address".
      //      iwp just checks that the link dialect ("iwp") matches the address info dialect,
      //      but that feels insufficient.  For now, just return the first endpoint we have;
      //      we should probably only have 1 for now anyway until we make ipv6 work.
      return &ep;
    }

    return nullptr;
  }

  //TODO: replace with control/data message sending with libquic
  bool
  LinkManager::SendTo(
      const RouterID& remote,
      const llarp_buffer_t& buf,
      ILinkSession::CompletionHandler completed,
      uint16_t priority)
  {
    if (stopping)
      return false;

    if (not HaveConnection(remote))
    {
      if (completed)
      {
        completed(ILinkSession::DeliveryStatus::eDeliveryDropped);
      }
      return false;
    }

    //TODO: send the message
    //TODO: if we keep bool return type, change this accordingly
    return false;
  }

  bool
  LinkManager::HaveConnection(const RouterID& remote, bool client_only) const
  {
    for (const auto& ep : endpoints)
    {
      if (auto itr = ep.connections.find(remote); itr != ep.connections.end())
      {
        if (not (itr->second.remote_is_relay and client_only))
          return true;
        return false;
      }
    }
    return false;
  }

  bool
  LinkManager::HaveClientConnection(const RouterID& remote) const
  {
    return HaveConnection(remote, true);
  }

  void
  LinkManager::DeregisterPeer(RouterID remote)
  {
    m_PersistingSessions.erase(remote);
    for (const auto& ep : endpoints)
    {
      if (auto itr = ep.connections.find(remote); itr != ep.connections.end())
      {
        /*
        itr->second.conn->close(); //TODO: libquic needs some function for this
        */
      }
    }

    LogInfo(remote, " has been de-registered");
  }

  void
  LinkManager::AddLink(const oxen::quic::opt::local_addr& bind, bool inbound)
  {
    //TODO: libquic callbacks: new_conn_alpn_notify, new_conn_pubkey_ok, new_conn_established/ready
    //      stream_opened, stream_data, stream_closed, conn_closed
    oxen::quic::dgram_data_callback dgram_cb = [this](oxen::quic::dgram_interface& dgi, bstring dgram){ HandleIncomingDataMessage(dgi, dgram); };
    auto ep = quic->endpoint(bind, std::move(dgram_cb), oxen::quic::opt::enable_datagrams{oxen::quic::Splitting::ACTIVE});
    endpoints.emplace_back();
    auto& endp = endpoints.back();
    endp.endpoint = std::move(ep);
    if (inbound)
    {
      endp.endpoint->listen(tls_creds);
      endp.inbound = true;
    }
  }

  void
  LinkManager::Stop()
  {
    if (stopping)
    {
      return;
    }

    util::Lock l(_mutex);

    LogInfo("stopping links");
    stopping = true;

    quic.reset();
  }

  void
  LinkManager::PersistSessionUntil(const RouterID& remote, llarp_time_t until)
  {
    if (stopping)
      return;

    util::Lock l(_mutex);

    m_PersistingSessions[remote] = std::max(until, m_PersistingSessions[remote]);
    if (HaveClientConnection(remote))
    {
      // mark this as a client so we don't try to back connect
      m_Clients.Upsert(remote);
    }
  }

  size_t
  LinkManager::NumberOfConnectedRouters(bool clients_only) const
  {
    size_t count{0};
    for (const auto& ep : endpoints)
    {
      for (const auto& conn : ep.connections)
      {
        if (not (conn.second.remote_is_relay and clients_only))
          count++;
      }
    }

    return count;
  }

  size_t
  LinkManager::NumberOfConnectedClients() const
  {
    return NumberOfConnectedRouters(true);
  }

  bool
  LinkManager::GetRandomConnectedRouter(RouterContact& router) const
  {
    std::unordered_map<RouterID, RouterContact> connectedRouters;

    for (const auto& ep : endpoints)
    {
      for (const auto& [router_id, conn] : ep.connections)
      {
        connectedRouters.emplace(router_id, conn.remote_rc);
      }
    }

    const auto sz = connectedRouters.size();
    if (sz)
    {
      auto itr = connectedRouters.begin();
      if (sz > 1)
      {
        std::advance(itr, randint() % sz);
      }

      router = itr->second;

      return true;
    }

    return false;
  }

  //TODO: this?  perhaps no longer necessary in the same way?
  void
  LinkManager::CheckPersistingSessions(llarp_time_t now)
  {
    if (stopping)
      return;
  }

  //TODO: do we still need this concept?
  void
  LinkManager::updatePeerDb(std::shared_ptr<PeerDb> peerDb)
  {
  }

  //TODO: this
  util::StatusObject
  LinkManager::ExtractStatus() const
  {
    return {};
  }

  void
  LinkManager::Init(I_RCLookupHandler* rcLookup)
  {
    stopping = false;
    _rcLookup = rcLookup;
    _nodedb = router->nodedb();
  }

  void
  LinkManager::Connect(RouterID router)
  {
    auto fn = [this](const RouterID& r, const RouterContact* const rc, const RCRequestResult res){
        if (res == RCRequestResult::Success)
          Connect(*rc);
        /* TODO:
        else
          RC lookup failure callback here
        */
    };

    _rcLookup->GetRC(router, fn);
  }

  // This function assumes the RC has already had its signature verified and connection is allowed.
  void
  LinkManager::Connect(RouterContact rc)
  {
    //TODO: connection failed callback
    if (HaveConnection(rc.pubkey))
      return;

    // RC shouldn't be valid if this is the case, but may as well sanity check...
    //TODO: connection failed callback
    if (rc.addrs.empty())
      return;

    //TODO: connection failed callback
    auto* ep = GetCompatibleLink(rc);
    if (ep == nullptr)
      return;

    //TODO: connection established/failed callbacks
    oxen::quic::stream_data_callback stream_cb = [this](oxen::quic::Stream& stream, bstring_view packet){ HandleIncomingControlMessage(stream, packet); };

    //TODO: once "compatible link" cares about address, actually choose addr to connect to
    //      based on which one is compatible with the link we chose.  For now, just use
    //      the first one.
    auto& selected = rc.addrs[0];
    oxen::quic::opt::remote_addr remote{selected.IPString(), selected.port};
    //TODO: confirm remote end is using the expected pubkey (RouterID).
    //TODO: ALPN for "client" vs "relay" (could just be set on endpoint creation)
    //TODO: does connect() inherit the endpoint's datagram data callback, and do we want it to if so?
    auto conn_interface = ep->endpoint->connect(remote, stream_cb, tls_creds);

    std::shared_ptr<oxen::quic::Stream> stream = conn_interface->get_new_stream();

    llarp::link::Connection conn;
    conn.conn = conn_interface;
    conn.control_stream = stream;
    conn.remote_rc = rc;
    conn.inbound = false;
    conn.remote_is_relay = true;

    ep->connections[rc.pubkey] = std::move(conn);
    ep->connid_map[conn_interface->scid()] = rc.pubkey;
  }

  void
  LinkManager::ConnectToRandomRouters(int numDesired)
  {
    std::set<RouterID> exclude;
    auto remainingDesired = numDesired;
    do
    {
      auto filter = [exclude](const auto& rc) -> bool { return exclude.count(rc.pubkey) == 0; };

      RouterContact other;
      if (const auto maybe = _nodedb->GetRandom(filter))
      {
        other = *maybe;
      }
      else
        break;

      exclude.insert(other.pubkey);
      if (not _rcLookup->SessionIsAllowed(other.pubkey))
        continue;

      Connect(other);
      --remainingDesired;
    } while (remainingDesired > 0);
  }

  void
  LinkManager::HandleIncomingDataMessage(oxen::quic::dgram_interface& dgi, bstring dgram)
  {
    //TODO: this
  }

  void
  LinkManager::HandleIncomingControlMessage(oxen::quic::Stream& stream, bstring_view packet)
  {
    //TODO: this
  }

}  // namespace llarp

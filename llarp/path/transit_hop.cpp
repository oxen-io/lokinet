#include "transit_hop.hpp"

#include <llarp/router/router.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp::path
{
  std::string
  TransitHopInfo::ToString() const
  {
    return fmt::format(
        "[TransitHopInfo tx={} rx={} upstream={} downstream={}]", txID, rxID, upstream, downstream);
  }

  TransitHop::TransitHop() : AbstractHopHandler{}
  {}

  void
  TransitHop::onion(ustring& data, SymmNonce& nonce, bool randomize) const
  {
    if (randomize)
      nonce.Randomize();
    nonce = crypto::onion(data.data(), data.size(), pathKey, nonce, nonceXOR);
  }

  void
  TransitHop::onion(std::string& data, SymmNonce& nonce, bool randomize) const
  {
    if (randomize)
      nonce.Randomize();
    nonce = crypto::onion(
        reinterpret_cast<unsigned char*>(data.data()), data.size(), pathKey, nonce, nonceXOR);
  }

  std::string
  TransitHop::onion_and_payload(
      std::string& payload, PathID_t next_id, std::optional<SymmNonce> nonce) const
  {
    SymmNonce n;
    auto& nref = nonce ? *nonce : n;
    onion(payload, nref, not nonce);

    return path::make_onion_payload(nref, next_id, payload);
  }

  bool
  TransitHop::send_path_control_message(std::string, std::string, std::function<void(std::string)>)
  {
    // TODO: if we want terminal/pivot hops to be able to *initiate* a request rather than
    //       simply responding/reacting to the client end's requests, this will need
    //       an implementation.
    return true;
  }

  bool
  TransitHop::Expired(llarp_time_t now) const
  {
    return destroy || (now >= ExpireTime());
  }

  llarp_time_t
  TransitHop::ExpireTime() const
  {
    return started + lifetime;
  }

  TransitHopInfo::TransitHopInfo(const RouterID& down) : downstream(down)
  {}

  /** Note: this is one of two places where AbstractRoutingMessage::bt_encode() is called, the
      other of which is llarp/path/path.cpp in Path::SendRoutingMessage(). For now,
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
  /* TODO: replace this with layer of onion + send data message
  bool
  TransitHop::SendRoutingMessage(std::string payload, Router* r)
  {
    if (!IsEndpoint(r->pubkey()))
      return false;

    TunnelNonce N;
    N.Randomize();
    // pad to nearest MESSAGE_PAD_SIZE bytes
    auto dlt = payload.size() % PAD_SIZE;

    if (dlt)
    {
      dlt = PAD_SIZE - dlt;
      // randomize padding
      crypto::randbytes(reinterpret_cast<uint8_t*>(payload.data()), dlt);
    }

    // TODO: relay message along

    return true;
  }
  */

  std::string
  TransitHop::ToString() const
  {
    return fmt::format(
        "[TransitHop {} started={} lifetime={}", info, started.count(), lifetime.count());
  }

  void
  TransitHop::Stop()
  {
    // TODO: still need this concept?
  }

  void
  TransitHop::SetSelfDestruct()
  {
    destroy = true;
  }

  void
  TransitHop::QueueDestroySelf(Router* r)
  {
    r->loop()->call([self = shared_from_this()] { self->SetSelfDestruct(); });
  }
}  // namespace llarp::path

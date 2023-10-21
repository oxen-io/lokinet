#include "abstracthophandler.hpp"

#include <llarp/router/router.hpp>

namespace llarp::path
{
  std::string
  make_onion_payload(
      const TunnelNonce& nonce, const PathID_t& path_id, const std::string_view& inner_payload)
  {
    return make_onion_payload(
        nonce,
        path_id,
        ustring_view{
            reinterpret_cast<const unsigned char*>(inner_payload.data()), inner_payload.size()});
  }

  std::string
  make_onion_payload(
      const TunnelNonce& nonce, const PathID_t& path_id, const ustring_view& inner_payload)
  {
    oxenc::bt_dict_producer next_dict;
    next_dict.append("NONCE", nonce.ToView());
    next_dict.append("PATHID", path_id.ToView());
    next_dict.append("PAYLOAD", inner_payload);

    return std::move(next_dict).str();
  }

  // handle data in upstream direction
  bool
  AbstractHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    auto& pkt = m_UpstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    r->TriggerPump();
    return true;
  }

  // handle data in downstream direction
  bool
  AbstractHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    auto& pkt = m_DownstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    r->TriggerPump();
    return true;
  }

  void
  AbstractHopHandler::DecayFilters(llarp_time_t now)
  {
    m_UpstreamReplayFilter.Decay(now);
    m_DownstreamReplayFilter.Decay(now);
  }
}  // namespace llarp::path

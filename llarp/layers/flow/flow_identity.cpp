#include "flow_identity.hpp"

#include <oxenc/bt_serialize.h>
#include <cstdint>
#include <llarp/crypto/crypto.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/util/logging.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <variant>
#include "llarp/layers/flow/flow_addr.hpp"
#include "llarp/layers/flow/flow_tag.hpp"
#include "llarp/service/convotag.hpp"

namespace llarp::layers::flow
{

  static auto logcat = log::Cat("flow-layer");

  FlowIdentityPrivateKeys
  FlowIdentityPrivateKeys::keygen()
  {
    FlowIdentityPrivateKeys keys{};

    auto crypto = CryptoManager::instance();

    crypto->identity_keygen(keys.identity);
    crypto->encryption_keygen(keys.encryption);
    crypto->pqe_keygen(keys.sntrup);
    if (not crypto->derive_subkey_private(keys.derivedKey, keys.identity, 1))
      throw std::runtime_error("failed to derive subkey");
    return keys;
  }

  const FlowAddr&
  FlowIdentityPrivateKeys::public_addr() const
  {
    if (not _root_pubkey)
      _root_pubkey = to_flow_addr(service::Address{identity.toPublic()});
    return _root_pubkey;
  }
  const dht::Key_t&
  FlowIdentityPrivateKeys::keyspace_location() const
  {
    if (_derived_pubkey.IsZero())
      derivedKey.toPublic(reinterpret_cast<PubKey&>(_derived_pubkey));
    return _derived_pubkey;
  }

  FlowIdentity::FlowIdentity(
      FlowLayer& parent, FlowAddr remote_addr, const FlowIdentityPrivateKeys& privkeys)
      : _parent{parent}
      , _local_privkeys{privkeys}
      , _state{nullptr}
      , _flow_info{privkeys.public_addr(), std::move(remote_addr), {}}
  {}

  bool
  FlowIdentity::operator==(const FlowIdentity& other) const
  {
    return flow_info == other.flow_info;
  }

  namespace
  {
    EndpointBase::AddressVariant_t
    to_addr_variant(const FlowAddr& addr)
    {
      switch (addr.kind())
      {
        case FlowAddr::Kind::snode:
          return RouterID{addr.as_array()};
        case FlowAddr::Kind::snapp:
          return service::Address{addr.as_array()};
        default:
          throw std::invalid_argument{
              "empty flow addr cannot be converted to EndpointBase::AddressVariant_t"};
      }
    }

    service::ProtocolType
    to_proto_type(const FlowDataKind& kind)
    {
      switch (kind)
      {
        case FlowDataKind::exit_ip_unicast:
          return service::ProtocolType::Exit;
        case FlowDataKind::direct_ip_unicast:
          // todo: change to direct after we swap to new
          // route layer.
          return service::ProtocolType::TrafficV4;
        case FlowDataKind::stream_unicast:
          return service::ProtocolType::QUIC;
        case FlowDataKind::auth:
          return service::ProtocolType::Auth;
        default:
          return service::ProtocolType::Control;
      }
    }
  }  // namespace

  /// wrapper type around deprecated logic.
  struct deprecated_path_ensure_handler
  {
    FlowIdentity& flow;
    FlowEstablish handshaker;

    void
    operator()(std::optional<service::ConvoTag> maybe_tag)
    {
      if (not maybe_tag)
      {
        log::warning(logcat, "flow establish to {} failed", flow.flow_info.dst);
        handshaker.fail("cannot establish flow");
        return;
      }
      auto& flow_info = flow._flow_info;
      flow_info.flow_tags.emplace(maybe_tag->as_array());
      log::debug(logcat, "flow established: {}", flow_info);
      handshaker.ready(flow_info);
    }
  };

  void
  FlowIdentity::async_ensure_flow(FlowEstablish handshaker)
  {
    const auto& ep = _parent.local_deprecated_loki_endpoint();
    if (not ep)
    {
      log::error(logcat, "no deprecated endpoint");
      handshaker.fail("no deprecated endpoint");
      return;
    }

    _parent.wakeup_send->Trigger();
    auto timeout = ep->PathAlignmentTimeout();

    log::info(logcat, "ensure flow to {}", flow_info.dst);

    ep->MarkAddressOutbound(to_addr_variant(flow_info.dst));

    auto ok = ep->EnsurePathTo(
        to_addr_variant(flow_info.dst), deprecated_path_ensure_handler{*this, handshaker}, timeout);
    if (ok)
      return;

    log::info(
        logcat,
        "failed to establish flow to {}: llarp::service::EnsurePathTo() failed",
        flow_info.dst);
    handshaker.fail("service::EnsurePathTo() failed");
  }

  void
  FlowIdentity::send_to_remote(std::vector<byte_t> data, FlowDataKind kind)
  {
    _parent.wakeup_send->Trigger();
    auto sz = data.size();
    log::trace(logcat, "send {} bytes of {} on {}", sz, kind, flow_info);
    auto ok = _parent.local_deprecated_loki_endpoint()->send_to_or_queue(
        to_addr_variant(flow_info.dst), std::move(data), to_proto_type(kind));
    if (not ok)
      log::warning(logcat, "failed to send {} bytes of {} on {}", sz, kind, flow_info);
  }

  void
  FlowIdentity::update_flow_tags(const std::set<FlowTag>& tags)
  {
    if (_flow_info.flow_tags != tags)
      _flow_info.flow_tags = tags;
  }

  void
  FlowIdentity::prune_flow_tags()
  {
    auto ep = _parent.local_deprecated_loki_endpoint();
    auto& tags = _flow_info.flow_tags;
    auto itr = tags.begin();

    while (itr != tags.end())
    {
      const service::ConvoTag tag{itr->as_array()};
      if (ep->HasConvoTag(tag))
        ++itr;
      else
        itr = tags.erase(itr);
    }
  }

}  // namespace llarp::layers::flow

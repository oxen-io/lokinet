#include "flow_identity.hpp"

#include <oxenc/bt_serialize.h>
#include <chrono>
#include <llarp/crypto/crypto.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <memory>
#include <stdexcept>
#include <variant>

namespace llarp::layers::flow
{
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
    if (_root_pubkey.IsZero())
      _root_pubkey = FlowAddr{service::Address{identity.toPublic()}};
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
      FlowLayer& parent,
      FlowAddr remote_addr,
      FlowTag flow_tag_,
      const FlowIdentityPrivateKeys& privkeys)
      : _parent{parent}
      , _local_privkeys{privkeys}
      , _state{}
      , flow_info{privkeys.public_addr(), std::move(remote_addr), std::move(flow_tag_)}
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
              "flow addr cannot be converted to EndpointBase::AddressVariant_t"};
      }
    }

    struct deprecated_path_ensure_handler
    {
      FlowEstablish handshaker;
      FlowInfo flow_info;

      void
      operator()(std::optional<service::ConvoTag> maybe_tag)
      {
        if (not maybe_tag)
        {
          handshaker.fail("cannot establish flow");
          return;
        }

        handshaker.ready(flow_info);
      }
    };
  }  // namespace

  void
  FlowIdentity::async_ensure_flow(FlowEstablish handshaker)
  {
    handshaker.fail("not wired up");
  }

  void
  FlowIdentity::send_to_remote(std::vector<byte_t>, FlowDataKind)
  {}

}  // namespace llarp::layers::flow

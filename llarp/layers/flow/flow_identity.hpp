#pragma once

#include <oxenc/bt_producer.h>
#include <oxenc/bt_serialize.h>
#include <llarp/crypto/types.hpp>
#include <llarp/dht/key.hpp>
#include <memory>
#include <string_view>

#include "flow_addr.hpp"
#include "flow_establish.hpp"
#include "flow_info.hpp"
#include "flow_state.hpp"
#include "flow_tag.hpp"
#include "flow_traffic.hpp"

namespace llarp::layers::flow
{

  class FlowLayer;

  /// private keys used at the flow layer that are persistable.
  /// this includes any kinds of ephemeral private keys.
  /// ephemeral keys do not need to be persisted.
  /// shared secrets not included.
  struct FlowIdentityPrivateKeys
  {
    PQKeyPair sntrup;
    SecretKey identity;
    SecretKey encryption;
    PrivateKey derivedKey;

    /// generate a new private key bundle for the flow layer.
    static FlowIdentityPrivateKeys
    keygen();

    /// get the public .loki or .snode address
    const FlowAddr&
    public_addr() const;

    /// get the blinded dht keyspace location
    const dht::Key_t&
    keyspace_location() const;

   private:
    mutable FlowAddr _root_pubkey;
    mutable dht::Key_t _derived_pubkey;
  };

  struct deprecated_path_ensure_handler;

  /// the local end of a one to one flow to a remote given a flow isolation metric (flow_tag)
  /// flows do not change their source/destination address or their flow tag/convo tag.
  /// note: historically path handovers were allowed, but going forward this is discontinued.
  class FlowIdentity
  {
    FlowLayer& _parent;
    const FlowIdentityPrivateKeys& _local_privkeys;
    /// internal state holder.
    /// effectively a pimpl to abstract away .loki vs .snode codepaths.
    std::unique_ptr<FlowState_Base> _state;

   protected:
    friend struct deprecated_path_ensure_handler;
    FlowInfo _flow_info;

   public:
    /// holds the local/remote flow layer address and any flow isolation metric
    const FlowInfo& flow_info{_flow_info};

    FlowIdentity(const FlowIdentity&) = delete;
    FlowIdentity(FlowIdentity&&) = delete;

    FlowIdentity(
        FlowLayer& parent, FlowAddr remote_addr, const FlowIdentityPrivateKeys& local_keys);

    /// ensure we have a flow to the remote endpoint.
    /// pass in a handshaker to do any additional steps after establishment.
    void
    async_ensure_flow(FlowEstablish handshaker);

    /// send flow traffic to the remote.
    void
    send_to_remote(std::vector<byte_t> dataum, FlowDataKind kind);

    bool
    operator==(const FlowIdentity& other) const;
  };
}  // namespace llarp::layers::flow

namespace std
{
  template <>
  struct hash<llarp::layers::flow::FlowIdentity>
  {
    size_t
    operator()(const llarp::layers::flow::FlowIdentity& ident) const
    {
      return std::hash<llarp::layers::flow::FlowInfo>{}(ident.flow_info);
    }
  };
}  // namespace std

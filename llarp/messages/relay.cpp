#include "relay.hpp"

#include <llarp/path/path_context.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bencode.hpp>

namespace llarp
{
  void
  RelayUpstreamMessage::clear()
  {
    pathid.Zero();
    enc.Clear();
    nonce.Zero();
    version = 0;
  }

  std::string
  RelayUpstreamMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("a", "u");
      btdp.append("p", pathid.ToView());
      btdp.append("v", llarp::constants::proto_version);
      btdp.append("x", std::string_view{reinterpret_cast<const char*>(enc.data()), enc.size()});
      btdp.append("y", std::string_view{reinterpret_cast<const char*>(nonce.data()), nonce.size()});
    }
    catch (...)
    {
      log::critical(link_cat, "Error: RelayUpstreamMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  RelayUpstreamMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("p", pathid, read, key, buf))
      return false;
    if (!BEncodeMaybeVerifyVersion("v", version, llarp::constants::proto_version, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("x", enc, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("y", nonce, read, key, buf))
      return false;
    return read;
  }

  bool
  RelayUpstreamMessage::handle_message(Router* r) const
  {
    path::HopHandler_ptr path = r->path_context().GetPath(pathid);
    path = path ? path : r->path_context().GetTransitHop(conn->remote_rc.router_id(), pathid);
    if (path)
    {
      return path->HandleUpstream(llarp_buffer_t(enc), nonce, r);
    }
    return false;
  }

  void
  RelayDownstreamMessage::clear()
  {
    pathid.Zero();
    enc.Clear();
    nonce.Zero();
    version = 0;
  }

  std::string
  RelayDownstreamMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("a", "d");
      btdp.append("p", pathid.ToView());
      btdp.append("v", llarp::constants::proto_version);
      btdp.append("x", std::string_view{reinterpret_cast<const char*>(enc.data()), enc.size()});
      btdp.append("y", std::string_view{reinterpret_cast<const char*>(nonce.data()), nonce.size()});
    }
    catch (...)
    {
      log::critical(link_cat, "Error: RelayDownstreamMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  RelayDownstreamMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("p", pathid, read, key, buf))
      return false;
    if (!BEncodeMaybeVerifyVersion("v", version, llarp::constants::proto_version, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("x", enc, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("y", nonce, read, key, buf))
      return false;
    return read;
  }

  bool
  RelayDownstreamMessage::handle_message(Router* r) const
  {
    path::HopHandler_ptr path = r->path_context().GetPath(pathid);
    path = path ? path : r->path_context().GetTransitHop(conn->remote_rc.router_id(), pathid);
    if (path)
    {
      return path->HandleDownstream(llarp_buffer_t(enc), nonce, r);
    }
    llarp::LogWarn("no path for downstream message id=", pathid);
    return false;
  }
}  // namespace llarp

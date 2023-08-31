#include "intro.hpp"
#include <llarp/util/time.hpp>

namespace llarp::service
{
  util::StatusObject
  Introduction::ExtractStatus() const
  {
    util::StatusObject obj{
        {"router", router.ToHex()},
        {"path", path_id.ToHex()},
        {"expiresAt", to_json(expiry)},
        {"latency", to_json(latency)},
        {"version", uint64_t(version)}};
    return obj;
  }

  bool
  Introduction::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("k", router, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("l", latency, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("p", path_id, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("x", expiry, read, key, buf))
      return false;
    return read;
  }

  void
  Introduction::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("k", router.ToView());
      btdp.append("l", latency.count());
      btdp.append("p", path_id.ToView());
      btdp.append("v", version);
      btdp.append("x", expiry.count());
    }
    catch (...)
    {
      log::critical(intro_cat, "Error: Introduction failed to bt encode contents!");
    }
  }

  void
  Introduction::Clear()
  {
    router.Zero();
    path_id.Zero();
    latency = 0s;
    expiry = 0s;
  }

  std::string
  Introduction::ToString() const
  {
    return fmt::format(
        "[Intro k={} l={} p={} v={} x={}]",
        RouterID{router},
        latency.count(),
        path_id,
        version,
        expiry.count());
  }

}  // namespace llarp::service

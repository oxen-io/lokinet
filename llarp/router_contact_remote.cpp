#include "constants/version.hpp"
#include "crypto/crypto.hpp"
#include "net/net.hpp"
#include "router_contact.hpp"
#include "util/bencode.hpp"
#include "util/buffer.hpp"
#include "util/file.hpp"
#include "util/time.hpp"

#include <oxenc/bt_serialize.h>

namespace llarp
{
  RemoteRC::RemoteRC(std::string payload)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{payload};
      bt_load(btdc);
      bt_verify(btdc);
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "Failed to parse RemoteRC: {}", e.what());
      throw;
    }
  }

  void
  RemoteRC::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    (void)btdp;

    // TODO: implement append_encoded in oxenc so we can finish this implementation.
    // It is almost identical to the implementation of LocalRC::bt_encode, except the
    // call to append_signature is append_encoded.
    //
    // When that is done, we can take the common logic and move it into the base class
    // ::bt_encode, and then have each derived class call into a different virtual method
    // that calls append_signature in the LocalRC and append_encoded in the RemoteRC
  }

  void
  RemoteRC::bt_verify(oxenc::bt_dict_consumer& data, bool reject_expired)
  {
    data.require_signature("~", [this, reject_expired](ustring_view msg, ustring_view sig) {
      if (sig.size() != 64)
        throw std::runtime_error{"Invalid signature: not 64 bytes"};

      if (is_expired(time_now_ms()) and reject_expired)
        throw std::runtime_error{"Unable to verify expired RemoteRC!"};

      // TODO: revisit if this is needed; detail from previous implementation
      const auto* net = net::Platform::Default_ptr();

      if (net->IsBogon(addr().in4()) and BLOCK_BOGONS)
      {
        auto err = "Unable to verify expired RemoteRC!";
        log::info(logcat, err);
        throw std::runtime_error{err};
      }

      if (not crypto::verify(router_id(), msg, sig))
        throw std::runtime_error{"Failed to verify RemoteRC"};

      _signed_payload = msg;
    });
  }
}  // namespace llarp

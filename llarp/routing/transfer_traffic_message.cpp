#include "transfer_traffic_message.hpp"

#include "handler.hpp"
#include <llarp/util/bencode.hpp>

#include <oxenc/endian.h>

namespace llarp::routing
{
  bool
  TransferTrafficMessage::PutBuffer(const llarp_buffer_t& buf, uint64_t counter)
  {
    if (buf.sz > MAX_EXIT_MTU)
      return false;
    enc_buf.emplace_back(buf.sz + 8);
    byte_t* ptr = enc_buf.back().data();
    oxenc::write_host_as_big(counter, ptr);
    ptr += 8;
    memcpy(ptr, buf.base, buf.sz);
    // 8 bytes encoding overhead and 8 bytes counter
    _size += buf.sz + 16;
    return true;
  }

  std::string
  TransferTrafficMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "I");
      btdp.append("P", static_cast<uint64_t>(protocol));
      btdp.append("S", sequence_number);
      btdp.append("V", version);

      {
        auto sublist = btdp.append_list("X");

        for (auto& b : enc_buf)
          sublist.append(std::string_view{reinterpret_cast<const char*>(b.data()), b.size()});
      }
    }
    catch (...)
    {
      log::critical(route_cat, "Error: PathLatencyMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  TransferTrafficMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("P", protocol, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictList("X", enc_buf, read, key, buf))
      return false;
    return read or bencode_discard(buf);
  }

  bool
  TransferTrafficMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleTransferTrafficMessage(*this, r);
  }

}  // namespace llarp::routing

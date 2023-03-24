#include "transfer_traffic_message.hpp"

#include "handler.hpp"
#include "llarp/layers/flow/flow_data_kind.hpp"
#include "llarp/layers/flow/flow_traffic.hpp"
#include "llarp/service/protocol_type.hpp"
#include <cstdint>
#include <llarp/util/bencode.hpp>
#include <vector>

#include <oxenc/endian.h>

namespace llarp
{
  namespace routing
  {

    std::vector<layers::flow::FlowTraffic>
    TransferTrafficMessage::to_flow_traffic() const
    {
      std::vector<layers::flow::FlowTraffic> trafs;
      for (const auto& pkt : pkts)
      {
        if (auto sz = pkt.size(); sz >= 8)
        {
          sz -= 8;
          auto& traf = trafs.emplace_back();
          traf.datum.resize(sz);
          std::copy_n(pkt.data() + 8, sz, traf.datum.begin());
          traf.kind = protocol == service::ProtocolType::QUIC
              ? layers::flow::FlowDataKind::stream_unicast
              : layers::flow::FlowDataKind::direct_ip_unicast;
        }
      }
      return trafs;
    }

    bool
    TransferTrafficMessage::PutBuffer(const llarp_buffer_t& buf, uint64_t counter)
    {
      if (buf.sz > MaxExitMTU)
        return false;
      X.emplace_back(buf.sz + 8);
      byte_t* ptr = X.back().data();
      oxenc::write_host_as_big(counter, ptr);
      ptr += 8;
      memcpy(ptr, buf.base, buf.sz);
      // 8 bytes encoding overhead and 8 bytes counter
      _size += buf.sz + 16;
      return true;
    }

    bool
    TransferTrafficMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if (!BEncodeWriteDictInt("P", protocol, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictList("X", X, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    TransferTrafficMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("P", protocol, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictList("X", X, read, key, buf))
        return false;
      return read or bencode_discard(buf);
    }

    bool
    TransferTrafficMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleTransferTrafficMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp

#include "protocol_utils.hpp"

#include <llarp/util/endian.hpp>

#include <map>
#include <deque>

namespace llarp::service
{
  std::vector<OwnedBuffer>
  Unpack(OwnedBuffer packed)
  {
    const auto* ptr = packed.buf.get();
    size_t remains = packed.sz;

    std::vector<OwnedBuffer> many;
    while (remains >= 2)
    {
      size_t sz = bufbe16toh(ptr);
      ptr += 2;
      remains -= 2;
      if (sz < remains)
      {
        if (sz > 0)
          many.emplace_back(ptr, sz);
        ptr += sz;
        remains -= sz;
      }
    }
    return many;
  }

  std::vector<OwnedBuffer>
  PackAll(std::vector<OwnedBuffer> packets)
  {
    constexpr auto quantize = 128;
    constexpr auto buckets = 12;
    constexpr auto mtu = buckets * quantize;
    std::map<int, std::deque<OwnedBuffer>> packed;
    // partition into buckets based on thier size
    for (auto& pkt : packets)
    {
      // drop big packets
      if (pkt.sz > mtu)
        continue;
      // select bucket
      auto bucket = pkt.sz / quantize;
      // put it in the bucket
      packed[bucket].emplace_back(std::move(pkt));
    }
    // make all the packed packets
    std::vector<OwnedBuffer> toSend;
    for (auto& [bucket, packets] : packed)
    {
      (void)bucket;
      std::vector<byte_t> currentPacked;
      while (not packets.empty())
      {
        auto& packet = packets.front();
        if (currentPacked.size() + (2 + packet.sz) > mtu)
        {
          // would be too big so append a terminator to our current packet and send add it to the
          // packed vector to send
          currentPacked.emplace_back(0);
          currentPacked.emplace_back(0);
          toSend.emplace_back(currentPacked.data(), currentPacked.size());
          currentPacked.clear();
        }
        // put size
        auto& pos = currentPacked.emplace_back();
        currentPacked.emplace_back();
        htobe16buf(&pos, static_cast<uint16_t>(packet.sz));
        // put data
        const auto* ptr = packet.buf.get();
        currentPacked.insert(currentPacked.cend(), ptr, ptr + packet.sz);
        packets.pop_front();
      }
      // we are done adding packets
      currentPacked.emplace_back(0);
      currentPacked.emplace_back(0);
      toSend.emplace_back(currentPacked.data(), currentPacked.size());
    }
    return toSend;
  }

}  // namespace llarp::service

#include "intro.hpp"

namespace llarp
{
  namespace service
  {
    util::StatusObject
    Introduction::ExtractStatus() const
    {
      util::StatusObject obj{
          {"router", router.ToHex()},
          {"path", pathID.ToHex()},
          {"expiresAt", to_json(expiresAt)},
          {"latency", to_json(latency)},
          {"version", uint64_t(version)}};
      return obj;
    }

    bool
    Introduction::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictEntry("k", router, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("l", latency, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("p", pathID, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("x", expiresAt, read, key, buf))
        return false;
      return read;
    }

    bool
    Introduction::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      if (!BEncodeWriteDictEntry("k", router, buf))
        return false;
      if (latency > 0s)
      {
        if (!BEncodeWriteDictInt("l", latency.count(), buf))
          return false;
      }
      if (!BEncodeWriteDictEntry("p", pathID, buf))
        return false;
      if (!BEncodeWriteDictInt("v", version, buf))
        return false;
      if (!BEncodeWriteDictInt("x", expiresAt.count(), buf))
        return false;
      return bencode_end(buf);
    }

    void
    Introduction::Clear()
    {
      router.Zero();
      pathID.Zero();
      latency = 0s;
      expiresAt = 0s;
    }

    std::ostream&
    Introduction::print(std::ostream& stream, int level, int spaces) const
    {
      const RouterID r{router};
      Printer printer(stream, level, spaces);
      printer.printAttribute("k", r.ToString());
      printer.printAttribute("l", latency.count());
      printer.printAttribute("p", pathID);
      printer.printAttribute("v", version);
      printer.printAttribute("x", expiresAt.count());

      return stream;
    }
  }  // namespace service
}  // namespace llarp

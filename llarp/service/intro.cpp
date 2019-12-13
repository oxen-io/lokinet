#include <service/intro.hpp>

namespace llarp
{
  namespace service
  {
    util::StatusObject
    Introduction::ExtractStatus() const
    {
      util::StatusObject obj{{"router", router.ToHex()},
                             {"expiresAt", expiresAt},
                             {"latency", latency},
                             {"version", uint64_t(version)}};
      return obj;
    }

    bool
    Introduction::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("k", router, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("l", latency, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("p", pathID, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("x", expiresAt, read, key, buf))
        return false;
      return read;
    }

    bool
    Introduction::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      if(!BEncodeWriteDictEntry("k", router, buf))
        return false;
      if(latency)
      {
        if(!BEncodeWriteDictInt("l", latency, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("p", pathID, buf))
        return false;
      if(!BEncodeWriteDictInt("v", version, buf))
        return false;
      if(!BEncodeWriteDictInt("x", expiresAt, buf))
        return false;
      return bencode_end(buf);
    }

    void
    Introduction::Clear()
    {
      router.Zero();
      pathID.Zero();
      latency   = 0;
      expiresAt = 0;
    }

    std::ostream&
    Introduction::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("k", RouterID(router));
      printer.printAttribute("l", latency);
      printer.printAttribute("p", pathID);
      printer.printAttribute("v", version);
      printer.printAttribute("x", expiresAt);

      return stream;
    }
  }  // namespace service
}  // namespace llarp

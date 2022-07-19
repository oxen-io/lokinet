#include "srv_data.hpp"
#include <llarp/util/str.hpp>
#include <llarp/util/logging.hpp>

#include <limits>

#include <oxenc/bt_serialize.h>
#include "llarp/util/bencode.h"
#include "llarp/util/types.hpp"

namespace llarp::dns
{
  bool
  SRVData::IsValid() const
  {
    // if target is of first two forms outlined above
    if (target == "." or target.size() == 0)
    {
      return true;
    }

    // check target size is not absurd
    if (target.size() > TARGET_MAX_SIZE)
    {
      LogWarn("SRVData target larger than max size (", TARGET_MAX_SIZE, ")");
      return false;
    }

    // does target end in .loki?
    size_t pos = target.find(".loki");
    if (pos != std::string::npos && pos == (target.size() - 5))
    {
      return true;
    }

    // does target end in .snode?
    pos = target.find(".snode");
    if (pos != std::string::npos && pos == (target.size() - 6))
    {
      return true;
    }

    // if we're here, target is invalid
    LogWarn("SRVData invalid");
    return false;
  }

  SRVTuple
  SRVData::toTuple() const
  {
    return std::make_tuple(service_proto, priority, weight, port, target);
  }

  SRVData
  SRVData::fromTuple(SRVTuple tuple)
  {
    SRVData s;

    std::tie(s.service_proto, s.priority, s.weight, s.port, s.target) = std::move(tuple);

    return s;
  }

  bool
  SRVData::fromString(std::string_view srvString)
  {
    LogDebug("SRVData::fromString(\"", srvString, "\")");

    // split on spaces, discard trailing empty strings
    auto splits = split(srvString, " ", false);

    if (splits.size() != 5 && splits.size() != 4)
    {
      LogWarn("SRV record should have either 4 or 5 space-separated parts");
      return false;
    }

    service_proto = splits[0];

    if (not parse_int(splits[1], priority))
    {
      LogWarn("SRV record failed to parse \"", splits[1], "\" as uint16_t (priority)");
      return false;
    }

    if (not parse_int(splits[2], weight))
    {
      LogWarn("SRV record failed to parse \"", splits[2], "\" as uint16_t (weight)");
      return false;
    }

    if (not parse_int(splits[3], port))
    {
      LogWarn("SRV record failed to parse \"", splits[3], "\" as uint16_t (port)");
      return false;
    }

    if (splits.size() == 5)
      target = splits[4];
    else
      target = "";

    return IsValid();
  }

  bool
  SRVData::BEncode(llarp_buffer_t* buf) const
  {
    const std::string data = oxenc::bt_serialize(toTuple());
    return buf->write(data.begin(), data.end());
  }

  bool
  SRVData::BDecode(llarp_buffer_t* buf)
  {
    byte_t* begin = buf->cur;
    if (not bencode_discard(buf))
      return false;
    byte_t* end = buf->cur;
    std::string_view srvString{
        reinterpret_cast<char*>(begin), static_cast<std::size_t>(end - begin)};
    try
    {
      SRVTuple tuple{};
      oxenc::bt_deserialize(srvString, tuple);
      *this = fromTuple(std::move(tuple));
      return IsValid();
    }
    catch (const oxenc::bt_deserialize_invalid&)
    {
      return false;
    };
  }

  util::StatusObject
  SRVData::ExtractStatus() const
  {
    return util::StatusObject{
        {"proto", service_proto},
        {"priority", priority},
        {"weight", weight},
        {"port", port},
        {"target", target}};
  }
}  // namespace llarp::dns

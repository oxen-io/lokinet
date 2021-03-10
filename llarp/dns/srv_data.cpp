#include "srv_data.hpp"
#include <llarp/util/str.hpp>
#include <llarp/util/logging/logger.hpp>

#include <limits>

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

}  // namespace llarp::dns

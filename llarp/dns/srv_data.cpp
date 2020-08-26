#include <dns/srv_data.hpp>

#include <limits>

namespace llarp::dns
{

  bool SRVData::IsValid() const
  {
    // if target is of first two forms outlined above
    if (target == "." or target.size() == 0)
    {
      return true;
    }

    // check target size is not absurd
    if (target.size() > TARGET_MAX_SIZE)
    {
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
    return false;
  }

  SRVTuple SRVData::toTuple() const
  {
    return std::make_tuple(service_proto, priority, weight, port, target);
  }

  SRVData SRVData::fromTuple(SRVTuple tuple)
  {
    SRVData s;

    std::tie(s.service_proto, s.priority, s.weight, s.port, s.target) = std::move(tuple);

    return s;
  }

  bool SRVData::fromString(const std::string& srvString)
  {
    size_t prev = 0;

    size_t pos = srvString.find(" ");
    if (pos == std::string::npos)
    {
      return false;
    }
    service_proto = srvString.substr(prev, pos - prev);

    prev = pos+1;
    pos = srvString.find(" ", prev);
    if (pos == std::string::npos)
    {
      return false;
    }
    unsigned long number_from_string;
    number_from_string = std::stoul(srvString.substr(prev, pos - prev));
    if (number_from_string > std::numeric_limits<uint16_t>::max())
    {
      return false;
    }
    priority = number_from_string;

    prev = pos+1;
    pos = srvString.find(" ", prev);
    if (pos == std::string::npos)
    {
      return false;
    }
    number_from_string = std::stoul(srvString.substr(prev, pos - prev));
    if (number_from_string > std::numeric_limits<uint16_t>::max())
    {
      return false;
    }
    weight = number_from_string;

    prev = pos+1;
    pos = srvString.find(" ", prev);
    if (pos == std::string::npos)
    {
      return false;
    }
    number_from_string = std::stoul(srvString.substr(prev, pos - prev));
    if (number_from_string > std::numeric_limits<uint16_t>::max())
    {
      return false;
    }
    port = number_from_string;

    // after final space, interpret rest as target
    if (srvString.size() <= pos+1)
    {
      return false;
    }
    target = srvString.substr(pos);

    return IsValid();
  }

} // namespace llarp::dns

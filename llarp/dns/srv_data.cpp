#include <dns/srv_data.hpp>

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

} // namespace llarp::dns

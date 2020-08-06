#pragma once

#include <dns/name.hpp>
#include <dns/serialize.hpp>

namespace llarp::dns
{

  struct SRVData
  {
    static constexpr size_t TARGET_MAX_SIZE = 200;

    std::string service_proto; // service and protocol may as well be together

    uint16_t priority;
    uint16_t weight;
    uint16_t port;

    // target string for the SRV record to point to
    // options:
    //   empty                     - refer to query name
    //   dot                       - authoritative "no such service available"
    //   any other .loki or .snode - target is that .loki or .snode
    std::string target;

    // do some basic validation on the target string
    // note: this is not a conclusive, regex solution,
    // but rather some sanity/safety checks
    bool IsValid() const;

  };

} // namespace llarp::dns

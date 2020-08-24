#pragma once

#include <dns/name.hpp>
#include <dns/serialize.hpp>

#include <tuple>

namespace llarp::dns
{

  typedef std::tuple<std::string, uint16_t, uint16_t, uint16_t, std::string> SRVTuple;

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

    SRVTuple toTuple() const;

    static SRVData fromTuple(SRVTuple tuple);
  };

} // namespace llarp::dns

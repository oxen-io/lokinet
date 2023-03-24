#pragma once

#include "llarp/util/str.hpp"
#include "name.hpp"
#include "serialize.hpp"

#include <tuple>
#include <string_view>
#include <type_traits>
#include <utility>

#include "llarp/util/status.hpp"

namespace llarp::dns
{
  using SRVTuple = std::tuple<std::string, uint16_t, uint16_t, uint16_t, std::string>;

  class SRVData
  {
    static constexpr size_t TARGET_MAX_SIZE = 200;

    /* bind-like formatted string for SRV records in config file
     *
     * format:
     *   srv=service.proto priority weight port target
     *
     * exactly one space character between parts.
     *
     * if target is not empty, it must be either
     *  - simply a full stop (dot/period) OR
     *  - a name within the .loki or .snode subdomains. a target
     *    specified in this manner must not end with a full stop.
     *  - a full .loki or .snode address ending with a full stop.
     *  - the literal character '@' in which the primary flow layer address
     *    be it .loki or .snode is used.
     *
     * if empty target is provided the trailing space after port MUST be omitted.
     * when the target is empty it is treated like '@'.
     */
    void
    from_string(std::string_view srvString);

    SRVTuple _tuple;

   public:
    explicit SRVData(SRVTuple tup);
    explicit SRVData(std::string_view str);

    SRVData() = default;
    SRVData(const SRVData&) = default;
    SRVData(SRVData&&) = default;

    SRVData&
    operator=(SRVData&& other);
    SRVData&
    operator=(const SRVData& other);

    std::string& service_proto{
        std::get<0>(_tuple)};  // service and protocol may as well be together

    uint16_t& priority{std::get<1>(_tuple)};
    uint16_t& weight{std::get<2>(_tuple)};
    uint16_t& port{std::get<3>(_tuple)};

    // target string for the SRV record to point to
    // options:
    //   empty                     - dns query's qname
    //   @                         - dns query's qname
    //   dot                       - authoritative "no such service available"
    //   any other .loki or .snode - target is that .loki or .snode
    std::string& target{std::get<4>(_tuple)};

    const SRVTuple& tuple{_tuple};

    // do some basic validation on the target string
    // note: this is not a conclusive, regex solution,
    // but rather some sanity/safety checks
    bool
    valid() const;

    /// return if the target refers to the query's qname.
    bool
    empty_target() const;

    /// returns true if the target is authoritative "no such service"
    bool
    target_full_stop() const;

    /// so we can put SRVData in a std::set
    constexpr bool
    operator<(const SRVData& other) const
    {
      return tuple < other.tuple;
    }

    constexpr bool
    operator==(const SRVData& other) const
    {
      return tuple == other.tuple;
    }

    bool
    BEncode(llarp_buffer_t*) const;

    bstring_t
    bt_serizalize() const;

    void
    bt_deserialize(byte_view_t& raw);

    bool
    BDecode(llarp_buffer_t*);

    util::StatusObject
    ExtractStatus() const;

    std::vector<std::string_view>
    target_dns_labels(std::string_view rootname) const;

    bstring_t
    encode_dns(std::string rootname) const;
  };

}  // namespace llarp::dns

namespace std
{
  template <>
  struct hash<llarp::dns::SRVData>
  {
    size_t
    operator()(const llarp::dns::SRVData& data) const
    {
      const std::hash<std::string> h_str{};
      const std::hash<uint16_t> h_port{};
      return h_str(data.service_proto) ^ (h_str(data.target) << 3) ^ (h_port(data.priority) << 5)
          ^ (h_port(data.weight) << 7) ^ (h_port(data.port) << 9);
    }
  };
}  // namespace std

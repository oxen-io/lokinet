#include "srv_data.hpp"
#include <cstdint>
#include <llarp/util/str.hpp>
#include <llarp/util/logging.hpp>

#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include <oxenc/bt_serialize.h>
#include <oxenc/endian.h>
#include "llarp/dns/name.hpp"
#include "llarp/dns/question.hpp"
#include "llarp/util/bencode.h"
#include "llarp/util/buffer.hpp"
#include "llarp/util/types.hpp"

namespace llarp::dns
{
  static auto logcat = log::Cat("dns");

  bool
  SRVData::valid() const
  {
    if (empty_target() or target_full_stop())
      return true;

    // check target size is not absurd
    if (target.size() > TARGET_MAX_SIZE)
      return false;

    for (auto suffix : {".loki"sv, ".loki."sv, ".snode"sv, ".snode."sv})
      if (ends_with(target, suffix))
        return true;

    return false;
  }

  SRVData::SRVData(SRVTuple tuple_) : _tuple{std::move(tuple_)}
  {}

  SRVData::SRVData(std::string_view str) : SRVData{}
  {
    from_string(std::move(str));
  }

  void
  SRVData::from_string(std::string_view str)
  {
    log::debug(logcat, "SRVData::fromString(\"{}\")", str);

    // split on spaces, discard trailing empty strings
    auto splits = split(str, " ", false);

    if (splits.size() != 5 && splits.size() != 4)
      throw std::invalid_argument{"SRV record should have either 4 or 5 space-separated parts"};

    service_proto = splits[0];

    if (not parse_int(splits[1], priority))
      throw std::invalid_argument{
          "SRV record failed to parse \"{}\" as uint16_t (priority)"_format(splits[1])};

    if (not parse_int(splits[2], weight))
      throw std::invalid_argument{
          "SRV record failed to parse \"{}\" as uint16_t (weight)"_format(splits[2])};

    if (not parse_int(splits[3], port))
      throw std::invalid_argument{
          "SRV record failed to parse \"{}\" as uint16_t (port)"_format(splits[3])};

    if (splits.size() == 5)
      target = splits[4];
    else
      target = "@";

    if (not valid())
      throw std::invalid_argument{"srv record is invalid"};
  }

  bool
  SRVData::BEncode(llarp_buffer_t* buf) const
  {
    auto data = bt_serizalize();
    return buf->write(data.begin(), data.end());
  }

  bstring_t
  SRVData::bt_serizalize() const
  {
    auto data = oxenc::bt_serialize(tuple);
    return bstring_t{reinterpret_cast<const byte_t*>(data.data()), data.size()};
  }

  void
  SRVData::bt_deserialize(byte_view_t& raw)
  {
    oxenc::bt_list_consumer bt{
        std::string_view{reinterpret_cast<const char*>(raw.data()), raw.size()}};
    bt.consume_list(_tuple);
    auto rem = bt.current_buffer();
    if (not valid())
      throw std::invalid_argument{"invalid srv data"};
    raw = raw.substr(raw.size() - rem.size());
  }

  SRVData&
  SRVData::operator=(SRVData&& other)
  {
    _tuple = std::move(other._tuple);
    return *this;
  }

  SRVData&
  SRVData::operator=(const SRVData& other)
  {
    _tuple = other.tuple;
    return *this;
  }

  bool
  SRVData::BDecode(llarp_buffer_t* buf)
  {
    byte_t* begin = buf->cur;
    if (not bencode_discard(buf))
      return false;
    byte_t* end = buf->cur;
    try
    {
      byte_view_t b{begin, static_cast<size_t>(end - begin)};
      bt_deserialize(b);
      return valid();
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

  bstring_t
  SRVData::encode_dns(std::string rootname) const
  {
    bstring_t buf;
    buf.resize(6);
    auto ptr = buf.data();
    oxenc::write_host_as_big(priority, ptr);
    ptr += 2;
    oxenc::write_host_as_big(weight, ptr);
    ptr += 2;
    oxenc::write_host_as_big(port, ptr);
    ptr += 2;
    buf += encode_dns_labels(target_dns_labels(std::move(rootname)));
    return buf;
  }

  std::vector<std::string_view>
  SRVData::target_dns_labels(std::string_view rootname) const
  {
    return split(empty_target() ? rootname : target, ".");
  }

  bool
  SRVData::empty_target() const
  {
    return target.empty() or target == "@";
  }

  bool
  SRVData::target_full_stop() const
  {
    return target == ".";
  }

}  // namespace llarp::dns

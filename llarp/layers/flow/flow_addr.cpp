#include "flow_addr.hpp"
#include <oxenc/base32z.h>
#include <stdexcept>

#include <llarp/util/str.hpp>

namespace llarp::layers::flow
{
  FlowAddr::Kind
  FlowAddr::get_kind(std::string_view addr_str)
  {
    if (ends_with(addr_str, ".loki"))
      return Kind::snapp;
    if (ends_with(addr_str, ".snode"))
      return Kind::snode;
    throw std::invalid_argument{"bad flow address: '{}'"_format(addr_str)};
  }
  namespace
  {
    llarp::AlignedBuffer<32>::Data
    decode_addr(std::string_view str)
    {
      llarp::AlignedBuffer<32>::Data data{};
      if (auto sz = oxenc::to_base32z_size(data.size()); sz != str.size())
        throw std::invalid_argument{"data decode size {} != {}"_format(sz, str.size())};
      oxenc::from_base32z(str.begin(), str.end(), data.begin());
      return data;
    }
  }  // namespace

  FlowAddr::FlowAddr(std::string str)
      : llarp::AlignedBuffer<32>{decode_addr(split(str, ".", true)[0])}, _kind{get_kind(str)}
  {}

  FlowAddr::FlowAddr(EndpointBase::AddressVariant_t addr)
      : FlowAddr{var::visit([](auto&& a) -> std::string { return a.ToString(); }, addr)}
  {}

  std::string
  FlowAddr::ToString() const
  {
    return oxenc::to_base32z(begin(), end())
        + (kind() == Kind::snapp       ? ".loki"
               : kind() == Kind::snode ? ".snode"
                                       : "");
  }

  bool
  FlowAddr::operator==(const FlowAddr& other) const
  {
    using parent_t = AlignedBuffer<SIZE>;
    return kind() == other.kind() and parent_t::operator==(static_cast<const parent_t&>(other));
  }

  FlowAddr::Kind
  FlowAddr::kind() const
  {
    return _kind;
  }
}  // namespace llarp::layers::flow

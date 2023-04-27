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

  FlowAddr::FlowAddr(std::string str) : _kind{get_kind(str)}
  {
    if (auto sz = oxenc::to_base32z_size(_data.size()); sz != str.size())
      throw std::invalid_argument{"data decode size {} != {}"_format(sz, str.size())};
    oxenc::from_base32z(str.begin(), str.end(), _data.begin());
  }

  FlowAddr::FlowAddr(Kind k) : FlowAddr{k, {}}
  {}

  FlowAddr::FlowAddr(Kind k, const backing_array_t& data) : _kind{k}, _data{data}

  {}

  FlowAddr::FlowAddr(FlowAddr&& other) : FlowAddr{other._kind, other._data}

  {
    other._kind = Kind::empty;
  }

  FlowAddr&
  FlowAddr::operator=(FlowAddr&& other) &
  {
    _kind = other._kind;
    _data = other._data;
    other._kind = Kind::empty;
    return *this;
  }

  std::string
  FlowAddr::ToString() const
  {
    if (_kind == Kind::empty)
      return "null";
    return oxenc::to_base32z(_data.begin(), _data.end())
        + (_kind == Kind::snapp       ? ".loki"
               : _kind == Kind::snode ? ".snode"
                                      : "");
  }

}  // namespace llarp::layers::flow

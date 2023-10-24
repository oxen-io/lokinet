#pragma once

#include <llarp/util/bencode.hpp>

#include <oxenc/bt.h>

namespace
{
  static auto policy_cat = llarp::log::Cat("lokinet.policy");
}  // namespace

namespace llarp::exit
{
  struct Policy
  {
    uint64_t proto = 0;
    uint64_t port = 0;
    uint64_t drop = 0;
    uint64_t version = llarp::constants::proto_version;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    decode_key(const llarp_buffer_t& k, llarp_buffer_t* val);

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const;
  };
}  // namespace llarp::exit

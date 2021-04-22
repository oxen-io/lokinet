#pragma once

#include "key.hpp"
#include <llarp/util/status.hpp>
#include <cstdint>

namespace llarp
{
  namespace dht
  {
    struct TXOwner
    {
      Key_t node;
      uint64_t txid = 0;

      TXOwner() = default;
      TXOwner(const TXOwner&) = default;
      TXOwner(TXOwner&&) = default;

      TXOwner&
      operator=(const TXOwner&) = default;

      TXOwner(const Key_t& k, uint64_t id) : node(k), txid(id)
      {}

      util::StatusObject
      ExtractStatus() const
      {
        util::StatusObject obj{
            {"txid", txid},
            {"node", node.ToHex()},
        };
        return obj;
      }

      bool
      operator==(const TXOwner& other) const
      {
        return std::tie(txid, node) == std::tie(other.txid, other.node);
      }

      bool
      operator<(const TXOwner& other) const
      {
        return std::tie(txid, node) < std::tie(other.txid, other.node);
      }
    };
  }  // namespace dht
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::dht::TXOwner>
  {
    std::size_t
    operator()(const llarp::dht::TXOwner& o) const noexcept
    {
      std::size_t sz2;
      memcpy(&sz2, o.node.data(), sizeof(std::size_t));
      return o.txid ^ (sz2 << 1);
    }
  };
}  // namespace std

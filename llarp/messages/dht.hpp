#pragma once

#include "common.hpp"

#include <llarp/dht/key.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct DHTMessage : public AbstractSerializable
  {};

  struct FindRouterMessage : public DHTMessage
  {
   private:
    RouterID target;
    bool is_iterative{false};
    bool is_exploratory{false};
    uint64_t tx_id{0};

   public:
    explicit FindRouterMessage(const RouterID& rid, bool is_itr, bool is_exp, uint64_t tx)
        : target{rid}, is_iterative{is_itr}, is_exploratory{is_exp}, tx_id{tx}
    {}

    std::string
    bt_encode() const override
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("A", "R");
        btdp.append("E", is_exploratory ? 1 : 0);
        btdp.append("I", is_iterative ? 1 : 0);
        btdp.append("K", target.ToView());
        btdp.append("T", tx_id);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindRouterMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }

    static std::string
    serialize(const RouterID& rid, bool is_itr, bool is_exp, uint64_t tx)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("A", "R");
        btdp.append("E", is_exp ? 1 : 0);
        btdp.append("I", is_itr ? 1 : 0);
        btdp.append("K", rid.ToView());
        btdp.append("T", tx);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindRouterMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  };
}  // namespace llarp

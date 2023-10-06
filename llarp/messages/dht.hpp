#pragma once

#include "common.hpp"

namespace llarp
{
  namespace FindRouterMessage
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto RETRY_EXP = "RETRY AS EXPLORATORY"sv;
    inline auto RETRY_ITER = "RETRY AS ITERATIVE"sv;
    inline auto RETRY_NEW = "RETRY WITH NEW RECIPIENT"sv;

    inline static std::string
    serialize(const RouterID& rid, bool is_iterative, bool is_exploratory, uint64_t tx_id)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("E", is_exploratory);
        btdp.append("I", is_iterative);
        btdp.append("K", rid.ToView());
        btdp.append("T", tx_id);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindRouterMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace FindRouterMessage

  namespace FindIntroMessage
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto NOT_FOUND = "NOT FOUND"sv;
    inline auto TIMED_OUT = "TIMED OUT"sv;
    inline auto INVALID_ORDER = "INVALID ORDER"sv;
    inline auto INSUFFICIENT_NODES = "INSUFFICIENT NODES"sv;

    inline static std::string
    serialize(
        const dht::Key_t& location, std::string tag, uint64_t txid, bool is_relayed, uint64_t order)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("N", tag);
        btdp.append("O", order);
        btdp.append("R", is_relayed ? 1 : 0);
        btdp.append("S", location.ToView());
        btdp.append("T", txid);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindIntroMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace FindIntroMessage

  namespace FindNameMessage
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto NOT_FOUND = "NOT FOUND"sv;

    inline static std::string
    serialize([[maybe_unused]] const dht::Key_t& from, dht::Key_t name_hash, uint64_t tx_id)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("H", name_hash.ToView());
        btdp.append("T", tx_id);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindNameMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace FindNameMessage

  namespace PublishIntroMessage
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto INVALID_INTROSET = "INVALID INTROSET"sv;
    inline auto EXPIRED = "EXPIRED INTROSET"sv;
    inline auto INSUFFICIENT = "INSUFFICIENT NODES"sv;
    inline auto INVALID_ORDER = "INVALID ORDER"sv;

    inline static std::string
    serialize(std::string introset, uint64_t relay_order, uint64_t is_relayed, uint64_t tx_id)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("I", introset);
        btdp.append("O", relay_order);
        btdp.append("R", is_relayed);
        btdp.append("T", tx_id);
      }
      catch (...)
      {
        log::error(link_cat, "Error: FindNameMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace PublishIntroMessage
}  // namespace llarp

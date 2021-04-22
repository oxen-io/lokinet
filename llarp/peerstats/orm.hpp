#pragma once

#include <sqlite_orm/sqlite_orm.h>

#include "types.hpp"

/// Contains some code to help deal with sqlite_orm in hopes of keeping other headers clean

namespace llarp
{
  inline auto
  initStorage(const std::string& file)
  {
    using namespace sqlite_orm;
    return make_storage(
        file,
        make_table(
            "peerstats",
            make_column("routerId", &PeerStats::routerId, primary_key(), unique()),
            make_column("numConnectionAttempts", &PeerStats::numConnectionAttempts),
            make_column("numConnectionSuccesses", &PeerStats::numConnectionSuccesses),
            make_column("numConnectionRejections", &PeerStats::numConnectionRejections),
            make_column("numConnectionTimeouts", &PeerStats::numConnectionTimeouts),
            make_column("numPathBuilds", &PeerStats::numPathBuilds),
            make_column("numPacketsAttempted", &PeerStats::numPacketsAttempted),
            make_column("numPacketsSent", &PeerStats::numPacketsSent),
            make_column("numPacketsDropped", &PeerStats::numPacketsDropped),
            make_column("numPacketsResent", &PeerStats::numPacketsResent),
            make_column("numDistinctRCsReceived", &PeerStats::numDistinctRCsReceived),
            make_column("numLateRCs", &PeerStats::numLateRCs),
            make_column("peakBandwidthBytesPerSec", &PeerStats::peakBandwidthBytesPerSec),
            make_column("longestRCReceiveInterval", &PeerStats::longestRCReceiveInterval),
            make_column("leastRCRemainingLifetime", &PeerStats::leastRCRemainingLifetime)));
  }

  using PeerDbStorage = decltype(initStorage(""));

}  // namespace llarp

/// "custom" types for sqlite_orm
/// reference: https://github.com/fnc12/sqlite_orm/blob/master/examples/enum_binding.cpp
namespace sqlite_orm
{
  /// llarp_time_t serialization
  template <>
  struct type_printer<llarp_time_t> : public integer_printer
  {};

  template <>
  struct statement_binder<llarp_time_t>
  {
    int
    bind(sqlite3_stmt* stmt, int index, const llarp_time_t& value)
    {
      return statement_binder<int64_t>().bind(stmt, index, value.count());
    }
  };

  template <>
  struct field_printer<llarp_time_t>
  {
    std::string
    operator()(const llarp_time_t& value) const
    {
      std::stringstream stream;
      stream << value.count();
      return stream.str();
    }
  };

  template <>
  struct row_extractor<llarp_time_t>
  {
    llarp_time_t
    extract(const char* row_value)
    {
      int64_t raw = static_cast<int64_t>(atoi(row_value));
      return llarp_time_t(raw);
    }

    llarp_time_t
    extract(sqlite3_stmt* stmt, int columnIndex)
    {
      auto str = sqlite3_column_text(stmt, columnIndex);
      return this->extract((const char*)str);
    }
  };

  /// RouterID serialization
  template <>
  struct type_printer<llarp::RouterID> : public text_printer
  {};

  template <>
  struct statement_binder<llarp::RouterID>
  {
    int
    bind(sqlite3_stmt* stmt, int index, const llarp::RouterID& value)
    {
      return statement_binder<std::string>().bind(stmt, index, value.ToString());
    }
  };

  template <>
  struct field_printer<llarp::RouterID>
  {
    std::string
    operator()(const llarp::RouterID& value) const
    {
      return value.ToString();
    }
  };

  template <>
  struct row_extractor<llarp::RouterID>
  {
    llarp::RouterID
    extract(const char* row_value)
    {
      llarp::RouterID id;
      if (not id.FromString(row_value))
        throw std::runtime_error("Invalid RouterID in sqlite3 database");

      return id;
    }

    llarp::RouterID
    extract(sqlite3_stmt* stmt, int columnIndex)
    {
      auto str = sqlite3_column_text(stmt, columnIndex);
      return this->extract((const char*)str);
    }
  };

}  // namespace sqlite_orm

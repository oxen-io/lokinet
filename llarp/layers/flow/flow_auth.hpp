#pragma once

#include <functional>
#include <string>
#include <optional>

#include <llarp/util/formattable.hpp>
#include <string_view>

namespace llarp::layers::flow
{

  /// which phase in authentication a flow is in while establishing.
  enum class FlowAuthPhase
  {
    /// we have not sent the auth to the remote yet.
    auth_req_not_sent,
    /// we sent our auth to the remote.
    auth_req_sent,
    /// the remote explicitly rejected our auth.
    auth_nack,
    /// the remote asked for auth.
    auth_more,
    /// the remote accepted our auth.
    auth_ok,
  };
  std::string_view
  ToString(FlowAuthPhase phase);

}  // namespace llarp::layers::flow

namespace llarp
{

  template <>
  inline constexpr bool IsToStringFormattable<layers::flow::FlowAuthPhase> = true;
}  // namespace llarp

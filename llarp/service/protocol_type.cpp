#include "protocol_type.hpp"

namespace llarp::service
{
  std::ostream&
  operator<<(std::ostream& o, ProtocolType t)
  {
    return o
        << (t == ProtocolType::Control           ? "Control"
                : t == ProtocolType::TrafficV4   ? "TrafficV4"
                : t == ProtocolType::TrafficV6   ? "TrafficV6"
                : t == ProtocolType::Exit        ? "Exit"
                : t == ProtocolType::Auth        ? "Auth"
                : t == ProtocolType::QUIC        ? "QUIC"
                : t == ProtocolType::PacketsV4   ? "PacketsV4"
                : t == ProtocolType::PacketsV6   ? "PacketsV6"
                : t == ProtocolType::PacketsExit ? "PacketsExit"
                                                 : "(unknown-protocol-type)");
  }

  std::optional<ProtocolType>
  BatchedProtoAsUnderlying(ProtocolType t)
  {
    switch (t)
    {
      case ProtocolType::PacketsExit:
        return ProtocolType::Exit;
      case ProtocolType::PacketsV4:
        return ProtocolType::TrafficV4;
      case ProtocolType::PacketsV6:
        return ProtocolType::TrafficV6;
      default:
        return std::nullopt;
    }
  }

  std::optional<ProtocolType>
  ToBatchedProto(ProtocolType t)
  {
    switch (t)
    {
      case ProtocolType::Exit:
        return ProtocolType::PacketsExit;
      case ProtocolType::TrafficV4:
        return ProtocolType::PacketsV4;
      case ProtocolType::TrafficV6:
        return ProtocolType::PacketsV6;
      default:
        return std::nullopt;
    }
  }

}  // namespace llarp::service

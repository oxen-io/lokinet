#include "protocol_type.hpp"

namespace llarp::service
{
  std::ostream&
  operator<<(std::ostream& o, ProtocolType t)
  {
    return o
        << (t == ProtocolType::Control         ? "Control"
                : t == ProtocolType::TrafficV4 ? "TrafficV4"
                : t == ProtocolType::TrafficV6 ? "TrafficV6"
                : t == ProtocolType::Exit      ? "Exit"
                : t == ProtocolType::Auth      ? "Auth"
                : t == ProtocolType::QUIC      ? "QUIC"
                                               : "(unknown-protocol-type)");
  }

}  // namespace llarp::service

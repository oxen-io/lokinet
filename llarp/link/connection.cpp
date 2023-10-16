#include "connection.hpp"

namespace llarp::link
{
  Connection::Connection(
      std::shared_ptr<oxen::quic::connection_interface>& c,
      std::shared_ptr<oxen::quic::BTRequestStream>& s,
      RouterContact& rc)
      : conn{c}, control_stream{s}, remote_rc{std::move(rc)}
  {}

}  // namespace llarp::link

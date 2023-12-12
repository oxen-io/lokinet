#include "connection.hpp"

namespace llarp::link
{
  Connection::Connection(
      const std::shared_ptr<oxen::quic::connection_interface>& c,
      std::shared_ptr<oxen::quic::BTRequestStream>& s)
      : conn{c}, control_stream{s}/* , remote_rc{std::move(rc)} */
  {}

}  // namespace llarp::link

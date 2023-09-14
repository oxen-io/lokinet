#include "connection.hpp"

namespace llarp::link
{
  Connection::Connection(
      std::shared_ptr<oxen::quic::connection_interface>& c, std::shared_ptr<oxen::quic::Stream>& s)
      : conn{c}, control_stream{s}
  {}

}  // namespace llarp::link

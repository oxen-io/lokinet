#include "rpc_server.hpp"

namespace llarp::rpc
{
  RpcServer::RpcServer(LMQ_ptr lmq, AbstractRouter* r) : m_LMQ(std::move(lmq)), m_Router(r)
  {
  }

  void RpcServer::AsyncServeRPC(std::string_view)
  {
    throw std::runtime_error("FIXME: implement llarp::rpc::RpcServer::AsyncServeRPC");
  }

}  // namespace llarp::rpc

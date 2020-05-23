#include "rpc_server.hpp"
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>
#include <constants/version.hpp>

namespace llarp::rpc
{
  RpcServer::RpcServer(LMQ_ptr lmq, AbstractRouter* r) : m_LMQ(std::move(lmq)), m_Router(r)
  {
  }

  void
  RpcServer::AsyncServeRPC(std::string url)
  {
    m_LMQ->listen_plain(std::move(url));
    m_LMQ->add_category("llarp", lokimq::AuthLevel::none)
        .add_request_command(
            "version", [](lokimq::Message& msg) { msg.send_reply(llarp::VERSION_FULL); })
        .add_request_command("status", [&](lokimq::Message& msg) {
          std::promise<std::string> result;
          LogicCall(m_Router->logic(), [&result, r = m_Router]() {
            const auto state = r->ExtractStatus();
            result.set_value(state.dump());
          });
          auto ftr = result.get_future();
          msg.send_reply(ftr.get());
        });
  }

}  // namespace llarp::rpc

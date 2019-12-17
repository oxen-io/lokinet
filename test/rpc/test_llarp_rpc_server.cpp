#include <exit/context.hpp>

#include <router/router.hpp>
#include <rpc/rpc.hpp>

#include <chrono>
#include <gtest/gtest.h>
#include <zmq.hpp>

using namespace std::chrono_literals;

struct RpcServerTest : public ::testing::Test
{
  RpcServerTest()
    : router(nullptr, nullptr, nullptr)
    , server(&router)
  {
  }

  llarp::Router router;
  llarp::rpc::Server server;
};

TEST_F(RpcServerTest, BasicStartStop)
{
  EXPECT_TRUE(server.Start());
  EXPECT_TRUE(server.Stop());
}

TEST_F(RpcServerTest, SocketConnection)
{
  EXPECT_TRUE(server.Start());

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REQ);

  LogInfo("Connecting to RPC server's ZMQ socket...");

  // we're really testing that this doesn't block [forever]...
  socket.connect("ipc:///tmp/lokinetrpc");
}

TEST_F(RpcServerTest, JsonRpcPing)
{
  EXPECT_TRUE(server.Start());

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REQ);

  LogInfo("Connecting to RPC server's ZMQ socket...");

  socket.connect("ipc:///tmp/lokinetrpc");

  std::string raw = R"(
    {
      "jsonrpc": "2.0",
      "id": 1,
      "method": "ping"
    }
  )";

  socket.send(raw.c_str(), raw.size());

  zmq::message_t reply;
  bool gotReply = false;

  // try reading socket repeatedly until result is ready
  absl::Time start = absl::Now();
  absl::Duration d = absl::Milliseconds(100);
  while (not gotReply and (absl::Now() - start < d)) {

    server.Tick(0);

    auto result = socket.recv(reply, zmq::recv_flags::dontwait);
    if (result.has_value())
      gotReply = true;
    else
      std::this_thread::sleep_for(10ms);
  }

  EXPECT_TRUE(gotReply);

  std::string replyStr((const char*)reply.data(), reply.size());
  nlohmann::json replyJson = nlohmann::json::parse(replyStr);

  nlohmann::json expected = R"(
    {
      "id":1,
      "jsonrpc":"2.0",
      "result":"pong"
    }
  )"_json;

  EXPECT_EQ(expected, replyJson);
}


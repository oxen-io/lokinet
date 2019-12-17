#include <gtest/gtest.h>

#include <rpc/json_rpc_dispatcher.hpp>

struct TestJsonRpcDispatcher : public ::testing::Test
{
};


TEST_F(TestJsonRpcDispatcher, TestNonJsonString)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  nlohmann::json response = dispatcher.processRaw("Not a JSON string");

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], nullptr);
	EXPECT_EQ(response["error"]["code"], -32700);
	EXPECT_EQ(response["error"]["message"], "Invalid JSON string");
	EXPECT_EQ(response["error"]["data"]["parseMessage"].type(), nlohmann::json::value_t::string);
}

TEST_F(TestJsonRpcDispatcher, TestProperString)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("foobar", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{false, "IT IS DANGEROUS TO GO ALONE"};
  });

  std::string raw = R"(
    {
      "jsonrpc": "2.0",
      "id": 1,
      "method": "foobar"
    }
  )";

  nlohmann::json response = dispatcher.processRaw(raw);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], 1);
	EXPECT_EQ(response["result"], "IT IS DANGEROUS TO GO ALONE");
}

TEST_F(TestJsonRpcDispatcher, TestHandlerReturnError)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("foobar", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{true, "FAIL"};
  });

  nlohmann::json request = {
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "foobar"},
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], 2);
	EXPECT_EQ(response["error"]["code"], -32603);
	EXPECT_EQ(response["error"]["message"], "Internal error");
	EXPECT_EQ(response["error"]["data"], "FAIL");
}

TEST_F(TestJsonRpcDispatcher, TestHandlerNotFound)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;

  nlohmann::json request = {
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "foobar"},
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], 1);
	EXPECT_EQ(response["error"]["code"], -32601);
	EXPECT_EQ(response["error"]["message"], "Method not found");
	EXPECT_EQ(response["error"]["data"]["method"], "foobar");
}

TEST_F(TestJsonRpcDispatcher, TestEmptyId)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("test", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{true, "OK"};
  });

  nlohmann::json request = {
    {"jsonrpc", "2.0"},
    {"method", "test"}
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], nullptr);
}

TEST_F(TestJsonRpcDispatcher, TestInvalidId)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("test", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{true, "OK"};
  });

  nlohmann::json request = {
    {"id", {"val", "foo"}},
    {"jsonrpc", "2.0"},
    {"method", "test"}
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], nullptr);
	EXPECT_EQ(response["error"]["code"], -32600);
	EXPECT_EQ(response["error"]["message"], "Invalid id type");
}

TEST_F(TestJsonRpcDispatcher, TestMissingJsonRpcSpecification)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("test", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{true, "OK"};
  });

  nlohmann::json request = {
    {"id", 1},
    {"method", "test"}
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], 1);
	EXPECT_EQ(response["error"]["code"], -32600);
	EXPECT_EQ(response["error"]["message"], "Request missing 'jsonrpc'");
}

TEST_F(TestJsonRpcDispatcher, TestInvalidJsonRpcSpecification)
{
  llarp::rpc::JsonRpcDispatcher dispatcher;
  dispatcher.setHandler("test", [](const nlohmann::json& params)
  {
    (void)params; // unused
    return llarp::rpc::response_t{true, "OK"};
  });

  nlohmann::json request = {
    {"id", 1},
    {"method", "test"},
    {"jsonrpc", "wrong"}
  };

  nlohmann::json response = dispatcher.process(request);

	EXPECT_EQ(response["jsonrpc"], "2.0");
	EXPECT_EQ(response["id"], 1);
	EXPECT_EQ(response["error"]["code"], -32600);
	EXPECT_EQ(response["error"]["message"], "Request must identify as \"jsonrpc\": \"2.0\"");
}

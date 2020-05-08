#include <libabyss.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <ev/ev.hpp>
#include <net/net.hpp>
#include <util/thread/threading.hpp>

#include <gtest/gtest.h>

struct AbyssTestBase : public ::testing::Test
{
  llarp::sodium::CryptoLibSodium crypto;
  llarp_ev_loop_ptr loop = nullptr;
  std::shared_ptr< llarp::Logic > logic;
  abyss::httpd::BaseReqHandler* server = nullptr;
  abyss::http::JSONRPC* client         = nullptr;
  const std::string method             = "test.method";
  bool called                          = false;

  AbyssTestBase()
  {
    llarp::SetLogLevel(llarp::eLogDebug);
  }

  void
  AssertMethod(const std::string& meth) const
  {
    ASSERT_EQ(meth, method);
  }

  void
  Start()
  {
    throw std::runtime_error("FIXME (replace libabyss with lokimq)");
    /*
    loop  = llarp_make_ev_loop();
    logic = std::make_shared< llarp::Logic >();
    loop->set_logic(logic);
    sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons((llarp::randint() % 2000) + 2000);
    addr.sin_family      = AF_INET;
    llarp::Addr a(addr);
    while(true)
    {
      if(server->ServeAsync(loop, logic, a))
      {
        client->RunAsync(loop, a.ToString());
        logic->call_later(1s, std::bind(&AbyssTestBase::Stop, this));
        return;
      }
      std::this_thread::sleep_for(1s);
    }
    */
  }

  void
  Stop()
  {
    llarp::LogDebug("test case Stop() called");
    llarp_ev_loop_stop(loop);
  }

  void
  AsyncStop()
  {
    LogicCall(logic, std::bind(&AbyssTestBase::Stop, this));
  }

  ~AbyssTestBase()
  {
    logic.reset();
    llarp::SetLogLevel(llarp::eLogInfo);
  }
};

struct ClientHandler : public abyss::http::IRPCClientHandler
{
  AbyssTestBase* test;
  ClientHandler(abyss::http::ConnImpl* impl, AbyssTestBase* parent)
      : abyss::http::IRPCClientHandler(impl), test(parent)
  {
  }

  void
  HandleError()
  {
    FAIL() << "unexpected error";
  }

  void
  PopulateReqHeaders(abyss::http::Headers_t& /*hdr*/)
  {
  }

  bool
  HandleResponse(abyss::http::RPC_Response /*response*/)
  {
    test->AsyncStop();
    return true;
  }
};

struct ServerHandler : public abyss::httpd::IRPCHandler
{
  AbyssTestBase* test;
  ServerHandler(abyss::httpd::ConnImpl* impl, AbyssTestBase* parent)
      : abyss::httpd::IRPCHandler(impl), test(parent)
  {
  }

  bool
  ValidateHost(const std::string & /*hostname */) const override
  {
    return true;
  }
  
  Response
  HandleJSONRPC(Method_t method, const Params& /*params*/)
  {
    test->AssertMethod(method);
    test->called = true;
    return Response();
  }

  ~ServerHandler()
  {
  }
};

struct AbyssTest : public AbyssTestBase,
                   public abyss::http::JSONRPC,
                   public abyss::httpd::BaseReqHandler
{
  AbyssTest()
      : AbyssTestBase()
      , abyss::http::JSONRPC()
      , abyss::httpd::BaseReqHandler(1s)
  {
    client = this;
    server = this;
  }

  abyss::http::IRPCClientHandler*
  NewConn(abyss::http::ConnImpl* impl)
  {
    return new ClientHandler(impl, this);
  }

  abyss::httpd::IRPCHandler*
  CreateHandler(abyss::httpd::ConnImpl* impl)
  {
    return new ServerHandler(impl, this);
  }

  void
  AsyncFlush()
  {
    LogicCall(logic, std::bind(&AbyssTest::Flush, this));
  }

  void
  RunLoop()
  {
    llarp_ev_loop_run_single_process(loop, logic);
  }
};

TEST_F(AbyssTest, TestClientAndServer)
{
#ifdef WIN32
  GTEST_SKIP();
#else
  Start();
  QueueRPC(method, nlohmann::json::object(),
           std::bind(&AbyssTest::NewConn, this, std::placeholders::_1));

  AsyncFlush();
  RunLoop();
  ASSERT_TRUE(called);
#endif
}

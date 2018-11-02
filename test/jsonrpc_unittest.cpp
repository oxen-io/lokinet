#include <gtest/gtest.h>
#include <libabyss.hpp>
#include <llarp/ev.h>
#include <llarp/threading.hpp>
#include <llarp/net.hpp>

struct AbyssTestBase : public ::testing::Test
{
  llarp_crypto crypto;
  llarp_threadpool* threadpool         = nullptr;
  llarp_ev_loop* loop                  = nullptr;
  llarp_logic* logic                   = nullptr;
  abyss::httpd::BaseReqHandler* server = nullptr;
  abyss::http::JSONRPC* client         = nullptr;
  const std::string method             = "test.method";

  void
  AssertMethod(const std::string& meth) const
  {
    ASSERT_TRUE(meth == method);
  }

  void
  SetUp()
  {
    // for llarp_randint
    llarp_crypto_init(&crypto);
  }

  void
  Start()
  {
    threadpool = llarp_init_same_process_threadpool();
    llarp_ev_loop_alloc(&loop);
    logic = llarp_init_single_process_logic(threadpool);

    sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons((llarp_randint() % 2000) + 2000);
    addr.sin_family      = AF_INET;
    llarp::Addr a(addr);
    while(true)
    {
      if(server->ServeAsync(loop, logic, a))
      {
        client->RunAsync(loop, a.ToString());
        return;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  void
  Stop()
  {
    if(server)
      server->Close();
    llarp_logic_stop(logic);
    llarp_ev_loop_stop(loop);
    llarp_threadpool_stop(threadpool);
  }

  void
  TearDown()
  {
    if(loop && threadpool && logic)
    {
      llarp_free_logic(&logic);
      llarp_ev_loop_free(&loop);
      llarp_free_threadpool(&threadpool);
    }
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
    ASSERT_TRUE(false);
  }

  void
  PopulateReqHeaders(abyss::http::Headers_t& hdr)
  {
  }

  bool
  HandleResponse(const abyss::http::RPC_Response& response)
  {
    test->Stop();
    return true;
  }
};

struct ServerHandler : public abyss::httpd::IRPCHandler
{
  const AbyssTestBase* test;
  ServerHandler(abyss::httpd::ConnImpl* impl, const AbyssTestBase* parent)
      : abyss::httpd::IRPCHandler(impl), test(parent)
  {
  }

  bool
  HandleJSONRPC(Method_t method, const Params& params, Response& response)
  {
    test->AssertMethod(method);
    return true;
  }
};

struct AbyssTest : public AbyssTestBase,
                   public abyss::http::JSONRPC,
                   public abyss::httpd::BaseReqHandler
{
  AbyssTest()
      : AbyssTestBase()
      , abyss::http::JSONRPC()
      , abyss::httpd::BaseReqHandler(1000)

  {
  }

  abyss::http::IRPCClientHandler*
  NewConn(abyss::http::ConnImpl* impl)
  {
    return new ClientHandler(impl, this);
  }

  abyss::httpd::IRPCHandler*
  CreateHandler(abyss::httpd::ConnImpl* impl) const
  {
    return new ServerHandler(impl, this);
  }

  void
  SetUp()
  {
    AbyssTestBase::SetUp();
    client = this;
    server = this;
  }

  static void
  FlushIt(void* u)
  {
    static_cast< AbyssTest* >(u)->Flush();
  }

  void
  AsyncFlush()
  {
    llarp_logic_queue_job(logic, {this, &FlushIt});
  }

  void
  RunLoop()
  {
    llarp_ev_loop_run_single_process(loop, threadpool, logic);
  }
};

TEST_F(AbyssTest, TestClientAndServer)
{
  Start();
  abyss::json::Value params;
  params.SetObject();
  QueueRPC(method, std::move(params),
           std::bind(&AbyssTest::NewConn, this, std::placeholders::_1));
  AsyncFlush();
  RunLoop();
};
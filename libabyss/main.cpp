#include <libabyss.hpp>
#include <llarp/net.hpp>
#ifndef _WIN32
#include <sys/signal.h>
#endif

struct DemoHandler : public abyss::httpd::IRPCHandler
{
  DemoHandler(abyss::httpd::ConnImpl* impl) : abyss::httpd::IRPCHandler(impl)
  {
  }

  bool
  HandleJSONRPC(Method_t method, __attribute__((unused)) const Params& params,
                Response& resp) override
  {
    llarp::LogInfo("method: ", method);
    resp.AddMember("result", abyss::json::Value().SetInt(1),
                   resp.GetAllocator());
    return true;
  }
};

struct DemoCall : public abyss::http::IRPCClientHandler
{
  DemoCall(abyss::http::ConnImpl* impl) : abyss::http::IRPCClientHandler(impl)
  {
    llarp::LogInfo("new call");
  }

  bool
  HandleResponse(abyss::http::RPC_Response resp) override
  {
    std::string body;
    abyss::json::ToString(resp, body);
    llarp::LogInfo("got response body: ", body);
    return true;
  }

  void
  PopulateReqHeaders(__attribute__((unused))
                     abyss::http::Headers_t& hdr) override
  {
  }

  void
  HandleError() override
  {
    llarp::LogError("error while handling call: ", strerror(errno));
  }
};

struct DemoClient : public abyss::http::JSONRPC
{
  abyss::http::IRPCClientHandler*
  NewConn(abyss::http::ConnImpl* impl)
  {
    return new DemoCall(impl);
  }

  void
  DoDemoRequest()
  {
    abyss::json::Value params;
    params.SetObject();
    QueueRPC("test", std::move(params),
             std::bind(&DemoClient::NewConn, this, std::placeholders::_1));
    Flush();
  };
};

struct DemoServer : public abyss::httpd::BaseReqHandler
{
  DemoServer() : abyss::httpd::BaseReqHandler(1000)
  {
  }

  abyss::httpd::IRPCHandler*
  CreateHandler(abyss::httpd::ConnImpl* impl)
  {
    return new DemoHandler(impl);
  }
};

int
main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
  // Ignore on Windows, we don't even get SIGPIPE (even though native *and*
  // emulated UNIX pipes exist - CreatePipe(2), pipe(3))
  // Microsoft libc only covers six signals
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#else
  WSADATA wsockd;
  int err;
  err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
  if(err)
  {
    perror("Failed to start Windows Sockets");
    return err;
  }
#endif
  llarp::SetLogLevel(llarp::eLogDebug);
  llarp_threadpool* threadpool = llarp_init_same_process_threadpool();
  llarp_ev_loop* loop          = nullptr;
  llarp_ev_loop_alloc(&loop);
  llarp::Logic* logic = new llarp::Logic(threadpool);
  sockaddr_in addr;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port        = htons(1222);
  addr.sin_family      = AF_INET;
  DemoServer serv;
  DemoClient client;
  llarp::Addr a(addr);
  while(true)
  {
    llarp::LogInfo("bind to ", a);
    if(serv.ServeAsync(loop, logic, a))
    {
      client.RunAsync(loop, a.ToString());
      client.DoDemoRequest();
      llarp_ev_loop_run_single_process(loop, threadpool, logic);
      return 0;
    }
    else
    {
      llarp::LogError("Failed to serve: ", strerror(errno));
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  return 0;
}

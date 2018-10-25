#include <libabyss.hpp>
#include <llarp/net.hpp>
#include <sys/signal.h>

struct DemoHandler : public abyss::http::IRPCHandler
{
  DemoHandler(abyss::http::ConnImpl* impl) : abyss::http::IRPCHandler(impl)
  {
  }

  bool
  HandleJSONRPC(const Method_t& method, Params params, Response& resp)
  {
    llarp::LogInfo("method: ", method);
    resp.AddMember("jsonrpc", abyss::json::Value().SetString("2.0"),
                   resp.GetAllocator());
    return true;
  }
};

struct DemoServer : public abyss::http::BaseReqHandler
{
  DemoServer() : abyss::http::BaseReqHandler(1000)
  {
  }

  abyss::http::IRPCHandler*
  CreateHandler(abyss::http::ConnImpl* impl) const
  {
    return new DemoHandler(impl);
  }
};

int
main(int argc, char* argv[])
{
  signal(SIGPIPE, SIG_IGN);
  llarp_threadpool* threadpool = llarp_init_same_process_threadpool();
  llarp_ev_loop* loop          = nullptr;
  llarp_ev_loop_alloc(&loop);
  llarp_logic* logic = llarp_init_single_process_logic(threadpool);
  sockaddr_in addr;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port        = htons(1222);
  addr.sin_family      = AF_INET;
  DemoServer serv;
  llarp::Addr a(addr);
  while(true)
  {
    llarp::LogInfo("bind to ", a);
    if(serv.ServeAsync(loop, logic, a))
    {
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

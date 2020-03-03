#include <libabyss.hpp>
#include <net/net.hpp>

#include <absl/synchronization/mutex.h>

#ifndef _WIN32
#include <signal.h>
#endif

struct DemoHandler : public abyss::httpd::IRPCHandler
{
  DemoHandler(abyss::httpd::ConnImpl* impl) : abyss::httpd::IRPCHandler(impl)
  {
  }

  nonstd::optional< Response >
  HandleJSONRPC(Method_t method, const Params& /*params*/) override
  {
    llarp::LogInfo("method: ", method);
    return nonstd::make_optional(Response::object());
  }
};

struct DemoCall : public abyss::http::IRPCClientHandler
{
  std::function< void(void) > m_Callback;
  std::shared_ptr< llarp::Logic > m_Logic;

  DemoCall(abyss::http::ConnImpl* impl, std::shared_ptr< llarp::Logic > logic,
           std::function< void(void) > callback)
      : abyss::http::IRPCClientHandler(impl)
      , m_Callback(callback)
      , m_Logic(logic)
  {
    llarp::LogInfo("new call");
  }

  bool HandleResponse(abyss::http::RPC_Response) override
  {
    llarp::LogInfo("response get");
    LogicCall(m_Logic, m_Callback);
    return true;
  }

  void
  PopulateReqHeaders(ABSL_ATTRIBUTE_UNUSED abyss::http::Headers_t& hdr) override
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
  llarp_ev_loop_ptr m_Loop;
  std::shared_ptr< llarp::Logic > m_Logic;

  DemoClient(llarp_ev_loop_ptr l, std::shared_ptr< llarp::Logic > logic)
      : abyss::http::JSONRPC(), m_Loop(std::move(l)), m_Logic(logic)
  {
  }

  abyss::http::IRPCClientHandler*
  NewConn(abyss::http::ConnImpl* impl)
  {
    return new DemoCall(impl, m_Logic, std::bind(&llarp_ev_loop_stop, m_Loop));
  }

  void
  DoDemoRequest()
  {
    QueueRPC("test", nlohmann::json::object(),
             std::bind(&DemoClient::NewConn, this, std::placeholders::_1));
    Flush();
  }
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
main(ABSL_ATTRIBUTE_UNUSED int argc, ABSL_ATTRIBUTE_UNUSED char* argv[])
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

#ifdef LOKINET_DEBUG
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
#endif
  llarp::SetLogLevel(llarp::eLogDebug);
  // Now that libuv is the single non-Windows event loop impl, we can
  // go back to using the normal function
  llarp_ev_loop_ptr loop = llarp_make_ev_loop();
  auto logic             = std::make_shared< llarp::Logic >();
  sockaddr_in addr;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port        = htons(1222);
  addr.sin_family      = AF_INET;
  DemoServer serv;
  DemoClient client(loop, logic);
  llarp::Addr a(addr);
  while(true)
  {
    llarp::LogInfo("bind to ", a);
    if(serv.ServeAsync(loop, logic, a))
    {
      client.RunAsync(loop, a.ToString());
      client.DoDemoRequest();
      llarp_ev_loop_run_single_process(loop, logic);
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

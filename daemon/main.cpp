#include <signal.h>
#include <llarp.hpp>
#include <memory>

std::unique_ptr< llarp::Context > ctx;

void
handle_signal(int sig)
{
  if(ctx)
    ctx->HandleSignal(sig);
}

int
main(int argc, char *argv[])
{
  const char *conffname = "daemon.ini";
  if(argc > 1)
    conffname = argv[1];

  ctx.reset(new llarp::Context(std::cout));

  signal(SIGINT, handle_signal);

  if(!ctx->LoadConfig(conffname))
    return 1;

  auto exitcode = ctx->Run();
  ctx->Close();
  return exitcode;
}

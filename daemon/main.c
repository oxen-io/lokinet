#include <llarp.h>
#include <signal.h>

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

#ifndef TESTNET
#define TESTNET false
#endif

int
main(int argc, char *argv[])
{
  const char *conffname = "daemon.ini";
  if(argc > 1)
    conffname = argv[1];
  ctx      = llarp_main_init(conffname, !TESTNET);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    code = llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  return code;
}

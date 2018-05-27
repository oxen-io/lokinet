#include <llarp.h>
#include <signal.h>
#include <memory>

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

int
main(int argc, char *argv[])
{
  const char *conffname = "daemon.ini";
  if(argc > 1)
    conffname = argv[1];

  if(llarp_main_init(&ctx, conffname))
  {
    signal(SIGINT, handle_signal);
    return llarp_main_run(ctx);
  }
}

#include <llarp.h>
#include <llarp/logger.h>
#include <signal.h>
#include <string>

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
  bool multiThreaded          = true;
  const char *singleThreadVar = getenv("LLARP_SHADOW");
  if(singleThreadVar && std::string(singleThreadVar) == "1")
  {
    multiThreaded = false;
  }
  const char *conffname = handleBaseCmdLineArgs(argc, argv);

  if(!llarp_ensure_config(conffname))
    return 1;

  ctx      = llarp_main_init(conffname, multiThreaded);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    code = llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  return code;
}

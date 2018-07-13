#include <getopt.h>
#include <llarp.h>
#include <llarp/logger.h>
#include <signal.h>
#include <sys/param.h>  // for MIN

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

#ifndef TESTNET
#define TESTNET 0
#endif

int
main(int argc, char *argv[])
{
  const char *conffname = "daemon.ini";
  int c;
  while(1)
  {
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"logLevel", required_argument, 0, 'o'},
        {0, 0, 0, 0}};
    int option_index = 0;
    c = getopt_long(argc, argv, "c:o:", long_options, &option_index);
    if(c == -1)
      break;
    switch(c)
    {
      case 0:
        break;
      case 'c':
        conffname = optarg;
        break;
      case 'o':
        if(strncmp(optarg, "debug", MIN(strlen(optarg), (unsigned long)5)) == 0)
        {
          cSetLogLevel(eLogDebug);
        }
        else if(strncmp(optarg, "info", MIN(strlen(optarg), (unsigned long)4))
                == 0)
        {
          cSetLogLevel(eLogInfo);
        }
        else if(strncmp(optarg, "warn", MIN(strlen(optarg), (unsigned long)4))
                == 0)
        {
          cSetLogLevel(eLogWarn);
        }
        else if(strncmp(optarg, "error", MIN(strlen(optarg), (unsigned long)5))
                == 0)
        {
          cSetLogLevel(eLogError);
        }
        break;
      default:
        abort();
    }
  }

  ctx      = llarp_main_init(conffname, !TESTNET);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    code = llarp_main_run(ctx);
    // llarp_main_free(ctx);
  }
  return code;
}

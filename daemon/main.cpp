#include <llarp.h>
#include <llarp/logger.hpp>
#include <signal.h>
#include <getopt.h>
#include <string>
#include <iostream>
#include <libgen.h>
#include "fs.hpp"

#ifdef _WIN32
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
#endif

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

int
printHelp(const char *argv0, int code = 1)
{
  std::cout << "usage: " << argv0 << " [-h] [-g] config.ini" << std::endl;
  return code;
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

  int opt            = 0;
  bool genconfigOnly = false;
  while((opt = getopt(argc, argv, "hg")) != -1)
  {
    switch(opt)
    {
      case 'h':
        return printHelp(argv[0], 0);
      case 'g':
        genconfigOnly = true;
        break;
      default:
        return printHelp(argv[0]);
    }
  }

  const char *conffname = nullptr;

  if(optind < argc)
  {
    // when we have an explicit filepath
    fs::path fname   = fs::path(argv[optind]);
    fs::path basedir = fname.parent_path();
    std::error_code ec;
    if(!fs::create_directories(basedir, ec))
    {
      if(ec)
      {
        llarp::LogError("failed to create '", basedir.string(),
                        "': ", ec.message());
        return 1;
      }
    }
    if(!llarp_ensure_config(argv[optind], basedir.string().c_str(),
                            genconfigOnly))
      return 1;
    conffname = argv[optind];
  }
  else
  {
// no explicit config file provided
#ifdef _WIN32
    fs::path homedir = fs::path(getenv("APPDATA"));
#else
    fs::path homedir = fs::path(getenv("HOME"));
#endif
    fs::path basepath = homedir / fs::path(".lokinet");
    fs::path fpath    = basepath / "lokinet.ini";

    std::error_code ec;
    if(!fs::create_directories(basepath, ec))
    {
      llarp::LogError("failed to create '", basepath.string(),
                      "': ", ec.message());
      return 1;
    }

    if(!llarp_ensure_config(fpath.string().c_str(), basepath.string().c_str(),
                            genconfigOnly))
      return 1;
    conffname = fpath.string().c_str();
  }

  if(genconfigOnly)
    return 0;

  ctx      = llarp_main_init(conffname, multiThreaded);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
#ifndef _WIN32
    signal(SIGHUP, handle_signal);
#endif
    code = llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  exit(code);
  return code;
}

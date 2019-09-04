#include <config/config.hpp>  // for ensure_config
#include <llarp.h>
#include <util/fs.hpp>
#include <util/logging/logger.hpp>

#include <csignal>

#if !defined(_WIN32) && !defined(__OpenBSD__)
#include <wordexp.h>
#endif

#include <cxxopts.hpp>
#include <string>
#include <iostream>

#ifdef _WIN32
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
extern "C" LONG FAR PASCAL
win32_signal_handler(EXCEPTION_POINTERS *);
#endif

struct llarp_main *ctx = 0;
std::promise< int > exit_code;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

#ifdef _WIN32
int
startWinsock()
{
  WSADATA wsockd;
  int err;
  err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
  if(err)
  {
    perror("Failed to start Windows Sockets");
    return err;
  }
  return 0;
}

extern "C" BOOL FAR PASCAL
handle_signal_win32(DWORD fdwCtrlType)
{
  UNREFERENCED_PARAMETER(fdwCtrlType);
  handle_signal(SIGINT);
  return TRUE;  // probably unreachable
}
#endif

/// resolve ~ and symlinks into actual paths (so we know the real path on disk,
/// to remove assumptions and confusion with permissions)
std::string
resolvePath(std::string conffname)
{
  // implemented in netbsd, removed downstream for security reasons
  // even though it is defined by POSIX.1-2001+
#if !defined(_WIN32) && !defined(__OpenBSD__)
  wordexp_t exp_result;
  wordexp(conffname.c_str(), &exp_result, 0);
  char *resolvedPath = realpath(exp_result.we_wordv[0], NULL);
  if(!resolvedPath)
  {
    // relative paths don't need to be resolved
    // llarp::LogWarn("Can't resolve path: ", exp_result.we_wordv[0]);
    return conffname;
  }
  return resolvedPath;
#else
  // TODO(despair): dig through LLVM local patch set
  // one of these exists deep in the bowels of LLVMSupport
  return conffname;  // eww, easier said than done outside of cygwin
#endif
}

/// this sets up, configures and runs the main context
static void
run_main_context(std::string conffname, bool multiThreaded, bool debugMode)
{
  // this is important, can downgrade from Info though
  llarp::LogDebug("Running from: ", fs::current_path().string());
  llarp::LogInfo("Using config file: ", conffname);
  ctx      = llarp_main_init(conffname.c_str(), multiThreaded);
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifndef _WIN32
    signal(SIGHUP, handle_signal);
#endif
    code = llarp_main_setup(ctx, debugMode);
    llarp::util::SetThreadName("llarp-mainloop");
    if(code == 0)
      code = llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  exit_code.set_value(code);
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

#ifdef _WIN32
  if(startWinsock())
    return -1;
  SetConsoleCtrlHandler(handle_signal_win32, TRUE);
  // SetUnhandledExceptionFilter(win32_signal_handler);
#endif

#ifdef LOKINET_DEBUG
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
#endif

  // clang-format off
  cxxopts::Options options(
		"lokinet",
		"LokiNET is a free, open source, private, decentralized, \"market based sybil resistant\" and IP based onion routing network"
    );
  options.add_options()
		("v,verbose", "Verbose", cxxopts::value<bool>())
		("h,help", "help", cxxopts::value<bool>())
		("g,generate", "generate client config", cxxopts::value<bool>())
		("r,router", "generate router config", cxxopts::value<bool>())
		("f,force", "overwrite", cxxopts::value<bool>())
		("d,debug", "debug mode - UNENCRYPTED TRAFFIC", cxxopts::value<bool>())
    ("config","path to configuration file", cxxopts::value<std::string>());

  options.parse_positional("config");
  // clang-format on

  bool genconfigOnly = false;
  bool asRouter      = false;
  bool overWrite     = false;
  bool debugMode     = false;
  std::string conffname;  // suggestions: confFName? conf_fname?

  try
  {
    auto result = options.parse(argc, argv);

    if(result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogDebug("debug logging activated");
    }

    if(result.count("help"))
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if(result.count("generate") > 0)
    {
      genconfigOnly = true;
    }

    if(result.count("debug") > 0)
    {
      debugMode = true;
    }

    if(result.count("force") > 0)
    {
      overWrite = true;
    }

    if(result.count("router") > 0)
    {
      asRouter = true;
      // we should generate and exit (docker needs this, so we don't write a
      // config each time on startup)
      genconfigOnly = true;
    }

    if(result.count("config") > 0)
    {
      auto arg = result["config"].as< std::string >();
      if(!arg.empty())
      {
        conffname = arg;
      }
    }
  }
  catch(const cxxopts::option_not_exists_exception &ex)
  {
    std::cerr << ex.what();
    std::cout << options.help() << std::endl;
    return 1;
  }

  if(!conffname.empty())
  {
    // when we have an explicit filepath
    fs::path fname   = fs::path(conffname);
    fs::path basedir = fname.parent_path();
    conffname        = fname.string();
    conffname        = resolvePath(conffname);
    std::error_code ec;

    // llarp::LogDebug("Basedir: ", basedir);
    if(basedir.string().empty())
    {
      // relative path to config

      // does this file exist?
      if(genconfigOnly)
      {
        if(!llarp_ensure_config(conffname.c_str(), basedir.string().c_str(),
                                overWrite, asRouter))
          return 1;
      }
      else
      {
        if(!fs::exists(fname, ec))
        {
          llarp::LogError("Config file not found ", conffname);
          return 1;
        }
      }
    }
    else
    {
      // absolute path to config
      if(!fs::create_directories(basedir, ec))
      {
        if(ec)
        {
          llarp::LogError("failed to create '", basedir.string(),
                          "': ", ec.message());
          return 1;
        }
      }
      if(genconfigOnly)
      {
        // find or create file
        if(!llarp_ensure_config(conffname.c_str(), basedir.string().c_str(),
                                overWrite, asRouter))
          return 1;
      }
      else
      {
        // does this file exist?
        if(!fs::exists(conffname, ec))
        {
          llarp::LogError("Config file not found ", conffname);
          return 1;
        }
      }
    }
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
    // I don't think this is necessary with this condition
    // conffname = resolvePath(conffname);

    llarp::LogDebug("Find or create ", basepath.string());
    std::error_code ec;
    // These paths are guaranteed to exist - $APPDATA or $HOME
    // so only create .lokinet/*
    if(!fs::create_directory(basepath, ec))
    {
      if(ec)
      {
        llarp::LogError("failed to create '", basepath.string(),
                        "': ", ec.message());
        return 1;
      }
    }

    // if using default INI file, we're create it even if you don't ask us too
    if(!llarp_ensure_config(fpath.string().c_str(), basepath.string().c_str(),
                            overWrite, asRouter))
      return 1;
    conffname = fpath.string();
  }

  if(genconfigOnly)
  {
    return 0;
  }

  std::thread main_thread{
      std::bind(&run_main_context, conffname, multiThreaded, debugMode)};
  auto ftr = exit_code.get_future();
  do
  {
    // do periodic non lokinet related tasks here
  } while(ftr.wait_for(std::chrono::seconds(1)) != std::future_status::ready);

  main_thread.join();

#ifdef _WIN32
  ::WSACleanup();
#endif
  const auto code = ftr.get();
  exit(code);
  return code;
}

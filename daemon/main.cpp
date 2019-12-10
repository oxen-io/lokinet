#include <config/config.hpp>  // for ensure_config
#include <constants/version.hpp>
#include <llarp.hpp>
#include <util/lokinet_init.h>
#include <util/fs.hpp>
#include <util/logging/logger.hpp>
#include <util/logging/ostream_logger.hpp>

#include <csignal>

#include <cxxopts.hpp>
#include <string>
#include <iostream>
#include <future>

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
  {
    llarp_main_signal(ctx, sig);
  }
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

/// this sets up, configures and runs the main context
static void
run_main_context(std::string conffname, llarp_main_runtime_opts opts)
{
  // this is important, can downgrade from Info though
  llarp::LogDebug("Running from: ", fs::current_path().string());
  llarp::LogInfo("Using config file: ", conffname);
  ctx      = llarp_main_init(conffname.c_str());
  int code = 1;
  if(ctx)
  {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifndef _WIN32
    signal(SIGHUP, handle_signal);
#endif
    code = llarp_main_setup(ctx);
    llarp::util::SetThreadName("llarp-mainloop");
    if(code == 0)
      code = llarp_main_run(ctx, opts);
  }
  exit_code.set_value(code);
}

int
main(int argc, char *argv[])
{
  auto result = Lokinet_INIT();
  if(result)
  {
    return result;
  }
  llarp_main_runtime_opts opts;
  const char *singleThreadVar = getenv("LLARP_SHADOW");
  if(singleThreadVar && std::string(singleThreadVar) == "1")
  {
    opts.singleThreaded = true;
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
		("version", "version", cxxopts::value<bool>())
		("g,generate", "generate client config", cxxopts::value<bool>())
		("r,router", "generate router config", cxxopts::value<bool>())
		("f,force", "overwrite", cxxopts::value<bool>())
		("c,colour", "colour output", cxxopts::value<bool>()->default_value("true"))
		("b,background", "background mode (start, but do not connect to the network)", cxxopts::value<bool>())
    ("config","path to configuration file", cxxopts::value<std::string>());

  options.parse_positional("config");
  // clang-format on

  bool genconfigOnly = false;
  bool asRouter      = false;
  bool overWrite     = false;
  std::string conffname;
  try
  {
    auto result = options.parse(argc, argv);

    if(result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogDebug("debug logging activated");
    }

    if(!result["colour"].as< bool >())
    {
      llarp::LogContext::Instance().logStream =
          std::make_unique< llarp::OStreamLogStream >(false, std::cerr);
    }

    if(result.count("help"))
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if(result.count("version"))
    {
      std::cout << llarp_version() << std::endl;
      return 0;
    }

    if(result.count("generate") > 0)
    {
      genconfigOnly = true;
    }

    if(result.count("background") > 0)
    {
      opts.background = true;
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

    if(!basedir.empty())
    {
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
    }

    if(genconfigOnly)
    {
      if(!llarp_ensure_config(conffname.c_str(), basedir.string().c_str(),
                              overWrite, asRouter))
        return 1;
    }
    else
    {
      std::error_code ec;
      if(!fs::exists(fname, ec))
      {
        llarp::LogError("Config file not found ", conffname);
        return 1;
      }
    }
  }
  else
  {
    auto basepath = llarp::GetDefaultConfigDir();

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

    auto fpath = llarp::GetDefaultConfigPath();

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

  std::thread main_thread{std::bind(&run_main_context, conffname, opts)};
  auto ftr = exit_code.get_future();
  do
  {
    // do periodic non lokinet related tasks here
    if(ctx != nullptr)
    {
      auto ctx_pp = llarp::Context::Get(ctx);
      if(ctx_pp != nullptr)
      {
        if(ctx_pp->IsUp() and not ctx_pp->LooksAlive())
        {
          for(const auto &wtf : {"you have been visited by the mascott of the "
                                 "deadlocked router.",
                                 "⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⣀⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠄⠄⠄⠄",
                                 "⠄⠄⠄⠄⠄⢀⣀⣀⡀⠄⠄⠄⡠⢲⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⠄⠄",
                                 "⠄⠄⠄⠔⣈⣀⠄⢔⡒⠳⡴⠊⠄⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⣿⣿⣧⠄⠄",
                                 "⠄⢜⡴⢑⠖⠊⢐⣤⠞⣩⡇⠄⠄⠄⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠄⠝⠛⠋⠐",
                                 "⢸⠏⣷⠈⠄⣱⠃⠄⢠⠃⠐⡀⠄⠄⠄⠄⠙⠻⢿⣿⣿⣿⣿⣿⣿⣿⡿⠛⠸⠄⠄⠄⠄",
                                 "⠈⣅⠞⢁⣿⢸⠘⡄⡆⠄⠄⠈⠢⡀⠄⠄⠄⠄⠄⠄⠉⠙⠛⠛⠛⠉⠉⡀⠄⠡⢀⠄⣀",
                                 "⠄⠙⡎⣹⢸⠄⠆⢘⠁⠄⠄⠄⢸⠈⠢⢄⡀⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠃⠄⠄⠄⠄⠄",
                                 "⠄⠄⠑⢿⠈⢆⠘⢼⠄⠄⠄⠄⠸⢐⢾⠄⡘⡏⠲⠆⠠⣤⢤⢤⡤⠄⣖⡇⠄⠄⠄⠄⠄",
                                 "⣴⣶⣿⣿⣣⣈⣢⣸⠄⠄⠄⠄⡾⣷⣾⣮⣤⡏⠁⠘⠊⢠⣷⣾⡛⡟⠈⠄⠄⠄⠄⠄⠄",
                                 "⣿⣿⣿⣿⣿⠉⠒⢽⠄⠄⠄⠄⡇⣿⣟⣿⡇⠄⠄⠄⠄⢸⣻⡿⡇⡇⠄⠄⠄⠄⠄⠄⠄",
                                 "⠻⣿⣿⣿⣿⣄⠰⢼⠄⠄⠄⡄⠁⢻⣍⣯⠃⠄⠄⠄⠄⠈⢿⣻⠃⠈⡆⡄⠄⠄⠄⠄⠄",
                                 "⠄⠙⠿⠿⠛⣿⣶⣤⡇⠄⠄⢣⠄⠄⠈⠄⢠⠂⠄⠁⠄⡀⠄⠄⣀⠔⢁⠃⠄⠄⠄⠄⠄",
                                 "⠄⠄⠄⠄⠄⣿⣿⣿⣿⣾⠢⣖⣶⣦⣤⣤⣬⣤⣤⣤⣴⣶⣶⡏⠠⢃⠌⠄⠄⠄⠄⠄⠄",
                                 "⠄⠄⠄⠄⠄⠿⠿⠟⠛⡹⠉⠛⠛⠿⠿⣿⣿⣿⣿⣿⡿⠂⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄",
                                 "⠠⠤⠤⠄⠄⣀⠄⠄⠄⠑⠠⣤⣀⣀⣀⡘⣿⠿⠙⠻⡍⢀⡈⠂⠄⠄⠄⠄⠄⠄⠄⠄⠄",
                                 "⠄⠄⠄⠄⠄⠄⠑⠠⣠⣴⣾⣿⣿⣿⣿⣿⣿⣇⠉⠄⠻⣿⣷⣄⡀⠄⠄⠄⠄⠄⠄⠄⠄",
                                 "file a bug report now or be cursed with this "
                                 "annoying image in your syslog for all time."})
          {
            LogError(wtf);
          }
          std::abort();
        }
      }
    }
  } while(ftr.wait_for(std::chrono::seconds(1)) != std::future_status::ready);

  main_thread.join();
  const auto code = ftr.get();
#ifdef _WIN32
  ::WSACleanup();
#endif
  if(ctx)
  {
    llarp_main_free(ctx);
  }
  return code;
}

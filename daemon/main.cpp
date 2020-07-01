#include <config/config.hpp>  // for ensure_config
#include <constants/version.hpp>
#include <llarp.hpp>
#include <util/lokinet_init.h>
#include <util/fs.hpp>
#include <util/logging/logger.hpp>
#include <util/logging/ostream_logger.hpp>
#include <util/str.hpp>
#include <util/thread/logic.hpp>

#include <csignal>

#include <cxxopts.hpp>
#include <string>
#include <iostream>
#include <future>

#ifdef USE_JEMALLOC
#include <new>
#include <jemalloc/jemalloc.h>

void*
operator new(std::size_t sz)
{
  void* ptr = malloc(sz);
  if (ptr)
    return ptr;
  else
    throw std::bad_alloc{};
}
void
operator delete(void* ptr) noexcept
{
  free(ptr);
}

void
operator delete(void* ptr, size_t) noexcept
{
  free(ptr);
}
#endif

#ifdef _WIN32
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
extern "C" LONG FAR PASCAL
win32_signal_handler(EXCEPTION_POINTERS*);
#endif

std::shared_ptr<llarp::Context> ctx;
std::promise<int> exit_code;

void
handle_signal(int sig)
{
  if (ctx)
    LogicCall(ctx->logic, std::bind(&llarp::Context::HandleSignal, ctx.get(), sig));
  else
    std::cerr << "Received signal " << sig << ", but have no context yet. Ignoring!" << std::endl;
}

#ifdef _WIN32
int
startWinsock()
{
  WSADATA wsockd;
  int err;
  err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
  if (err)
  {
    perror("Failed to start Windows Sockets");
    return err;
  }
  ::CreateMutex(nullptr, FALSE, "lokinet_win32_daemon");
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
run_main_context(const fs::path confFile, const llarp::RuntimeOptions opts)
{
  try
  {
    // this is important, can downgrade from Info though
    llarp::LogDebug("Running from: ", fs::current_path().string());
    llarp::LogInfo("Using config file: ", confFile);

    llarp::Config conf;
    conf.Load(confFile, opts.isRouter, confFile.parent_path());

    ctx = std::make_shared<llarp::Context>();
    ctx->Configure(opts, {}, confFile);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifndef _WIN32
    signal(SIGHUP, handle_signal);
#endif

    ctx->Setup(opts);

    llarp::util::SetThreadName("llarp-mainloop");

    auto result = ctx->Run(opts);
    exit_code.set_value(result);
  }
  catch (std::exception& e)
  {
    llarp::LogError("Fatal: caught exception while running: ", e.what());
    exit_code.set_exception(std::current_exception());
  }
  catch (...)
  {
    llarp::LogError("Fatal: caught non-standard exception while running");
    exit_code.set_exception(std::current_exception());
  }
}

int
main(int argc, char* argv[])
{
  auto result = Lokinet_INIT();
  if (result)
  {
    return result;
  }
  llarp::RuntimeOptions opts;

#ifdef _WIN32
  if (startWinsock())
    return -1;
  SetConsoleCtrlHandler(handle_signal_win32, TRUE);
  // SetUnhandledExceptionFilter(win32_signal_handler);
#endif
  cxxopts::Options options(
      "lokinet",
      "LokiNET is a free, open source, private, "
      "decentralized, \"market based sybil resistant\" "
      "and IP based onion routing network");
  options.add_options()("v,verbose", "Verbose", cxxopts::value<bool>())(
      "h,help", "help", cxxopts::value<bool>())("version", "version", cxxopts::value<bool>())(
      "g,generate", "generate client config", cxxopts::value<bool>())(
      "r,relay", "run as relay instead of client", cxxopts::value<bool>())(
      "f,force", "overwrite", cxxopts::value<bool>())(
      "c,colour", "colour output", cxxopts::value<bool>()->default_value("true"))(
      "b,background",
      "background mode (start, but do not connect to the network)",
      cxxopts::value<bool>())(
      "config", "path to configuration file", cxxopts::value<std::string>());

  options.parse_positional("config");

  bool genconfigOnly = false;
  bool overwrite = false;
  fs::path configFile;
  try
  {
    auto result = options.parse(argc, argv);

    if (result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogDebug("debug logging activated");
    }

    if (!result["colour"].as<bool>())
    {
      llarp::LogContext::Instance().logStream =
          std::make_unique<llarp::OStreamLogStream>(false, std::cerr);
    }

    if (result.count("help"))
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if (result.count("version"))
    {
      std::cout << llarp::VERSION_FULL << std::endl;
      return 0;
    }

    if (result.count("generate") > 0)
    {
      genconfigOnly = true;
    }

    if (result.count("background") > 0)
    {
      opts.background = true;
    }

    if (result.count("router") > 0)
    {
      opts.isRouter = true;
    }

    if (result.count("force") > 0)
    {
      overwrite = true;
    }

    if (result.count("config") > 0)
    {
      auto arg = result["config"].as<std::string>();
      if (!arg.empty())
      {
        configFile = arg;
      }
    }
  }
  catch (const cxxopts::option_not_exists_exception& ex)
  {
    std::cerr << ex.what();
    std::cout << options.help() << std::endl;
    return 1;
  }

  if (!configFile.empty())
  {
    // when we have an explicit filepath
    fs::path basedir = configFile.parent_path();

    if (genconfigOnly)
    {
      llarp::ensureConfig(basedir, configFile, overwrite, opts.isRouter);
    }
    else
    {
      std::error_code ec;
      if (!fs::exists(configFile, ec))
      {
        llarp::LogError("Config file not found ", configFile);
        return 1;
      }

      if (ec)
        throw std::runtime_error(llarp::stringify("filesystem error: ", ec));
    }
  }
  else
  {
    llarp::ensureConfig(
        llarp::GetDefaultDataDir(), llarp::GetDefaultConfigPath(), overwrite, opts.isRouter);
    configFile = llarp::GetDefaultConfigPath();
  }

  if (genconfigOnly)
  {
    return 0;
  }

  std::thread main_thread{std::bind(&run_main_context, configFile, opts)};
  auto ftr = exit_code.get_future();
  do
  {
    // do periodic non lokinet related tasks here
    if (ctx and ctx->IsUp() and not ctx->LooksAlive())
    {
      for (const auto& wtf : {"you have been visited by the mascott of the "
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
        llarp::LogContext::Instance().ImmediateFlush();
      }
      std::abort();
    }
  } while (ftr.wait_for(std::chrono::seconds(1)) != std::future_status::ready);

  main_thread.join();

  int code = 0;

  try
  {
    code = ftr.get();
  }
  catch (const std::exception& e)
  {
    std::cerr << "main thread threw exception: " << e.what() << std::endl;
    code = 1;
  }
  catch (...)
  {
    std::cerr << "main thread threw non-standard exception" << std::endl;
    code = 2;
  }

  llarp::LogContext::Instance().ImmediateFlush();
#ifdef _WIN32
  ::WSACleanup();
#endif
  if (ctx)
  {
    ctx.reset();
  }
  return code;
}

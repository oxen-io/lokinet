#include <oxenmq/oxenmq.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <future>
#include <vector>
#include <array>
#include <llarp/net/net.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

#ifdef _WIN32
// add the unholy windows headers for iphlpapi
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <strsafe.h>
#else
#include <sys/wait.h>
#endif

/// do a oxenmq request on an omq instance blocking style
/// returns a json object parsed from the result
std::optional<nlohmann::json>
OMQ_Request(
    oxenmq::OxenMQ& omq,
    const oxenmq::ConnectionID& id,
    std::string_view method,
    std::optional<nlohmann::json> args = std::nullopt)
{
  std::promise<std::optional<std::string>> result_promise;

  auto handleRequest = [&result_promise](bool success, std::vector<std::string> result) {
    if ((not success) or result.empty())
    {
      result_promise.set_value(std::nullopt);
      return;
    }
    result_promise.set_value(result[0]);
  };
  if (args.has_value())
  {
    omq.request(id, method, handleRequest, args->dump());
  }
  else
  {
    omq.request(id, method, handleRequest);
  }
  auto ftr = result_promise.get_future();
  const auto str = ftr.get();
  if (str.has_value())
    return nlohmann::json::parse(*str);
  return std::nullopt;
}

namespace
{

  struct command_line_options
  {
    // bool options
    bool verbose = false;
    bool help = false;
    bool vpnUp = false;
    bool vpnDown = false;
    bool printStatus = false;
    bool killDaemon = false;

    // string options
    std::string exitAddress;
    std::string rpc;
    std::string endpoint = "default";
    std::string token;
    std::optional<std::string> range;

    // oxenmq
    oxenmq::address rpcURL{"tcp://127.0.0.1:1190"};
    oxenmq::LogLevel logLevel = oxenmq::LogLevel::warn;
  };

  // Takes a code, prints a message, and returns the code.  Intended use is:
  //     return exit_error(1, "blah: {}", 42);
  // from within main().
  template <typename... T>
  [[nodiscard]] int
  exit_error(int code, const std::string& format, T&&... args)
  {
    fmt::print(format, std::forward<T>(args)...);
    fmt::print("\n");
    return code;
  }

  // Same as above, but with code omitted (uses exit code 1)
  template <typename... T>
  [[nodiscard]] int
  exit_error(const std::string& format, T&&... args)
  {
    return exit_error(1, format, std::forward<T>(args)...);
  }

}  // namespace

int
main(int argc, char* argv[])
{
  CLI::App cli{"lokiNET vpn control utility", "lokinet-vpn"};
  command_line_options options{};

  // flags: boolean values in command_line_options struct
  cli.add_flag("-v,--verbose", options.verbose, "Verbose");
  cli.add_flag("--up", options.vpnUp, "Put VPN up");
  cli.add_flag("--down", options.vpnDown, "Put VPN down");
  cli.add_flag("--status", options.printStatus, "Print VPN status and exit");
  cli.add_flag("-k,--kill", options.killDaemon, "Kill lokinet daemon");

  // options: string values in command_line_options struct
  cli.add_option("--exit", options.exitAddress, "Specify exit node address")->capture_default_str();
  cli.add_option("--endpoint", options.endpoint, "Endpoint to use")->capture_default_str();
  cli.add_option("--token", options.token, "Exit auth token to use")->capture_default_str();

  // options: oxenmq values in command_line_options struct
  cli.add_option("--rpc", options.rpc, "Specify RPC URL for lokinet")->capture_default_str();
  cli.add_option(
         "--log-level", options.logLevel, "Log verbosity level, see log levels for accepted values")
      ->type_name("LEVEL")
      ->capture_default_str();

  try
  {
    cli.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return cli.exit(e);
  }

  try
  {
    if (options.verbose)
      options.logLevel = oxenmq::LogLevel::debug;
  }
  catch (const CLI::OptionNotFound& e)
  {
    cli.exit(e);
  }
  catch (const CLI::Error& e)
  {
    cli.exit(e);
  };

  int numCommands = options.vpnUp + options.vpnDown + options.printStatus + options.killDaemon;

  switch (numCommands)
  {
    case 0:
      return exit_error(3, "One of --up/--down/--status/--kill must be specified");
    case 1:
      break;
    default:
      return exit_error(3, "Only one of --up/--down/--status/--kill may be specified");
  }

  if (options.vpnUp and options.exitAddress.empty())
    return exit_error("No exit address provided, must specify --exit <address>");

  oxenmq::OxenMQ omq{
      [](oxenmq::LogLevel lvl, const char* file, int line, std::string msg) {
        std::cout << lvl << " [" << file << ":" << line << "] " << msg << std::endl;
      },
      options.logLevel};

  omq.start();

  std::promise<bool> connectPromise;

  const auto connectionID = omq.connect_remote(
      options.rpc,
      [&connectPromise](auto) { connectPromise.set_value(true); },
      [&connectPromise](auto, std::string_view msg) {
        std::cout << "Failed to connect to lokinet RPC: " << msg << std::endl;
        connectPromise.set_value(false);
      });

  auto ftr = connectPromise.get_future();
  if (not ftr.get())
    return 1;

  if (options.killDaemon)
  {
    if (not OMQ_Request(omq, connectionID, "llarp.halt"))
      return exit_error("Call to llarp.halt failed");
    return 0;
  }

  if (options.printStatus)
  {
    const auto maybe_status = OMQ_Request(omq, connectionID, "llarp.status");
    if (not maybe_status)
      return exit_error("call to llarp.status failed");

    try
    {
      const auto& ep = maybe_status->at("result").at("services").at(options.endpoint);
      const auto exitMap = ep.at("exitMap");
      if (exitMap.empty())
      {
        std::cout << "no exits" << std::endl;
      }
      else
      {
        for (const auto& [range, exit] : exitMap.items())
        {
          std::cout << range << " via " << exit.get<std::string>() << std::endl;
        }
      }
    }
    catch (std::exception& ex)
    {
      return exit_error("failed to parse result: {}", ex.what());
    }
    return 0;
  }
  if (options.vpnUp)
  {
    nlohmann::json opts{{"address", options.exitAddress}, {"token", options.token}};
    if (options.range)
      opts["ip_range"] = *options.range;

    auto maybe_result = OMQ_Request(omq, connectionID, "llarp.map_exit", std::move(opts));

    if (not maybe_result)
      return exit_error("could not add exit");

    if (auto err_it = maybe_result->find("error");
        err_it != maybe_result->end() and not err_it.value().is_null())
    {
      return exit_error("{}", err_it.value());
    }
  }
  if (options.vpnDown)
  {
    nlohmann::json opts{{"unmap_exit", true}};
    if (options.range)
      opts["ip_range"] = *options.range;
    if (not OMQ_Request(omq, connectionID, "llarp.unmap_exit", std::move(opts)))
      return exit_error("failed to unmap exit");
  }

  return 0;
}

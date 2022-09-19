#include <oxenmq/oxenmq.h>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>
#include <future>
#include <vector>
#include <array>
#include <llarp/net/net.hpp>

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

int
main(int argc, char* argv[])
{
  cxxopts::Options opts("lokinet-vpn", "LokiNET vpn control utility");

  // clang-format off
  opts.add_options()
    ("v,verbose", "Verbose", cxxopts::value<bool>())
    ("h,help", "help", cxxopts::value<bool>())
    ("kill", "kill the daemon", cxxopts::value<bool>())
    ("up", "put vpn up", cxxopts::value<bool>())
    ("down", "put vpn down", cxxopts::value<bool>())
    ("exit", "specify exit node address", cxxopts::value<std::string>())
    ("rpc", "rpc url for lokinet", cxxopts::value<std::string>())
    ("endpoint", "endpoint to use", cxxopts::value<std::string>())
    ("token", "exit auth token to use", cxxopts::value<std::string>())
    ("auth", "exit auth token to use", cxxopts::value<std::string>())
    ("status", "print status and exit", cxxopts::value<bool>())
    ("range", "ip range to map", cxxopts::value<std::string>())
    ;
  // clang-format on
  oxenmq::address rpcURL("tcp://127.0.0.1:1190");
  std::string exitAddress;
  std::string endpoint = "default";
  std::string token;
  std::optional<std::string> range;
  oxenmq::LogLevel logLevel = oxenmq::LogLevel::warn;
  bool goUp = false;
  bool goDown = false;
  bool printStatus = false;
  bool killDaemon = false;
  try
  {
    const auto result = opts.parse(argc, argv);

    if (result.count("help") > 0)
    {
      std::cout << opts.help() << std::endl;
      return 0;
    }

    if (result.count("verbose") > 0)
    {
      logLevel = oxenmq::LogLevel::debug;
    }
    if (result.count("rpc") > 0)
    {
      rpcURL = oxenmq::address(result["rpc"].as<std::string>());
    }
    if (result.count("exit") > 0)
    {
      exitAddress = result["exit"].as<std::string>();
    }
    goUp = result.count("up") > 0;
    goDown = result.count("down") > 0;
    printStatus = result.count("status") > 0;
    killDaemon = result.count("kill") > 0;

    if (result.count("endpoint") > 0)
    {
      endpoint = result["endpoint"].as<std::string>();
    }
    if (result.count("token") > 0)
    {
      token = result["token"].as<std::string>();
    }
    if (result.count("auth") > 0)
    {
      token = result["auth"].as<std::string>();
    }
    if (result.count("range") > 0)
    {
      range = result["range"].as<std::string>();
    }
  }
  catch (const cxxopts::option_not_exists_exception& ex)
  {
    std::cerr << ex.what();
    std::cout << opts.help() << std::endl;
    return 1;
  }
  catch (std::exception& ex)
  {
    std::cout << ex.what() << std::endl;
    return 1;
  }
  if ((not goUp) and (not goDown) and (not printStatus) and (not killDaemon))
  {
    std::cout << opts.help() << std::endl;
    return 1;
  }
  if (goUp and exitAddress.empty())
  {
    std::cout << "no exit address provided" << std::endl;
    return 1;
  }

  oxenmq::OxenMQ omq{
      [](oxenmq::LogLevel lvl, const char* file, int line, std::string msg) {
        std::cout << lvl << " [" << file << ":" << line << "] " << msg << std::endl;
      },
      logLevel};

  omq.start();

  std::promise<bool> connectPromise;

  const auto connID = omq.connect_remote(
      rpcURL,
      [&connectPromise](auto) { connectPromise.set_value(true); },
      [&connectPromise](auto, std::string_view msg) {
        std::cout << "failed to connect to lokinet RPC: " << msg << std::endl;
        connectPromise.set_value(false);
      });

  auto ftr = connectPromise.get_future();
  if (not ftr.get())
  {
    return 1;
  }

  if (killDaemon)
  {
    const auto maybe = OMQ_Request(lmq, connID, "llarp.halt");
    if (not maybe.has_value())
    {
      std::cout << "call to llarp.admin.die failed" << std::endl;
      return 1;
    }
    return 0;
  }

  if (printStatus)
  {
    const auto maybe_status = OMQ_Request(lmq, connID, "llarp.status");
    if (not maybe_status.has_value())
    {
      std::cout << "call to llarp.status failed" << std::endl;
      return 1;
    }

    try
    {
      const auto& ep = maybe_status->at("result").at("services").at(endpoint);
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
      std::cout << "failed to parse result: " << ex.what() << std::endl;
      return 1;
    }
    return 0;
  }
  if (goUp)
  {
    nlohmann::json opts{{"exit", exitAddress}, {"token", token}};
    if (range)
      opts["range"] = *range;

    auto maybe_result = OMQ_Request(omq, connID, "llarp.exit", opts);

    if (not maybe_result.has_value())
    {
      std::cout << "could not add exit" << std::endl;
      return 1;
    }

    if (maybe_result->contains("error") and maybe_result->at("error").is_string())
    {
      std::cout << maybe_result->at("error").get<std::string>() << std::endl;
      return 1;
    }
  }
  if (goDown)
  {
    nlohmann::json opts{{"unmap", true}};
    if (range)
      opts["range"] = *range;
    OMQ_Request(omq, connID, "llarp.exit", std::move(opts));
  }

  return 0;
}

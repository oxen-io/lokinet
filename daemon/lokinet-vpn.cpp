#include <lokimq/lokimq.h>
#include <cxxopts.hpp>
#include <future>

int
main(int argc, char* argv[])
{
  cxxopts::Options opts("lokinet-vpn", "LokiNET vpn control utility");

  opts.add_options()("v,verbose", "Verbose", cxxopts::value<bool>())(
      "h,help", "help", cxxopts::value<bool>())("up", "put vpn up", cxxopts::value<bool>())(
      "down", "put vpn down", cxxopts::value<bool>())(
      "exit", "specify exit node address", cxxopts::value<std::string>())(
      "rpc", "rpc url for lokinet", cxxopts::value<std::string>());

  lokimq::address rpcURL("tcp://127.0.0.1:1190");
  std::string exitAddress;
  lokimq::LogLevel logLevel = lokimq::LogLevel::warn;
  bool goUp = false;
  bool goDown = false;
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
      logLevel = lokimq::LogLevel::debug;
    }
    if (result.count("rpc") > 0)
    {
      rpcURL = lokimq::address(result["rpc"].as<std::string>());
    }
    if (result.count("exit") > 0)
    {
      exitAddress = result["exit"].as<std::string>();
    }
    goUp = result.count("up") > 0;
    goDown = result.count("down") > 0;
  }
  catch (const cxxopts::option_not_exists_exception& ex)
  {
    std::cerr << ex.what();
    std::cout << opts.help() << std::endl;
    return 1;
  }
  if ((not goUp) and (not goDown))
  {
    std::cout << opts.help() << std::endl;
    return 1;
  }
  if (goUp and exitAddress.empty())
  {
    std::cout << "no exit address provided" << std::endl;
    return 1;
  }

  lokimq::LokiMQ lmq{[](lokimq::LogLevel lvl, const char* file, int line, std::string msg) {
                       std::cout << lvl << " [" << file << ":" << line << "] " << msg << std::endl;
                     },
                     logLevel};

  lmq.start();

  std::promise<bool> connectPromise;

  const auto connID = lmq.connect_remote(
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

  return 0;
}

#include <lokimq/lokimq.h>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>
#include <future>
#include <vector>
#include <array>
#include <net/net.hpp>

#ifdef _WIN32
// add the unholy windows headers for iphlpapi
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

/// do a lokimq request on an lmq instance blocking style
/// returns a json object parsed from the result
std::optional<nlohmann::json>
LMQ_Request(
    lokimq::LokiMQ& lmq,
    const lokimq::ConnectionID& id,
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
    lmq.request(id, method, handleRequest, args->dump());
  }
  else
  {
    lmq.request(id, method, handleRequest);
  }
  auto ftr = result_promise.get_future();
  const auto str = ftr.get();
  if (str.has_value())
    return nlohmann::json::parse(*str);
  return std::nullopt;
}

/// get every ip address that is a gateway that isn't owned by interface with name ifname
std::vector<std::string>
GetGatewaysNotOnInterface(std::string ifname);

/// add route to ipaddr via gateway ip
void
AddRoute(std::string ipaddr, std::string gateway);

/// delete route to ipaddr via gateway ip
void
DelRoute(std::string ipaddr, std::string gateway);

/// add default route via interface with name ifname
void
AddDefaultRouteViaInterface(std::string ifname);

/// delete default route via interface with name ifname
void
DelDefaultRouteViaInterface(std::string ifname);

int
main(int argc, char* argv[])
{
  cxxopts::Options opts("lokinet-vpn", "LokiNET vpn control utility");

  opts.add_options()("v,verbose", "Verbose", cxxopts::value<bool>())(
      "h,help", "help", cxxopts::value<bool>())("up", "put vpn up", cxxopts::value<bool>())(
      "down", "put vpn down", cxxopts::value<bool>())(
      "exit", "specify exit node address", cxxopts::value<std::string>())(
      "rpc", "rpc url for lokinet", cxxopts::value<std::string>())(
      "endpoint", "endpoint to use", cxxopts::value<std::string>())(
      "token", "exit auth token to use", cxxopts::value<std::string>());

  lokimq::address rpcURL("tcp://127.0.0.1:1190");
  std::string exitAddress;
  std::string endpoint = "default";
  std::optional<std::string> token;
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

    if (result.count("endpoint") > 0)
    {
      endpoint = result["endpoint"].as<std::string>();
    }
    if (result.count("token") > 0)
    {
      token = result["token"].as<std::string>();
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

  std::vector<std::string> firstHops;
  std::string ifname;

  const auto maybe_status = LMQ_Request(lmq, connID, "llarp.status");
  if (not maybe_status.has_value())
  {
    std::cout << "call to llarp.status failed" << std::endl;
    return 1;
  }

  try
  {
    // extract first hops
    const auto& links = maybe_status->at("result")["links"]["outbound"];
    for (const auto& link : links)
    {
      const auto& sessions = link["sessions"]["established"];
      for (const auto& session : sessions)
      {
        std::string addr = session["remoteAddr"];
        const auto pos = addr.find(":");
        firstHops.push_back(addr.substr(0, pos));
      }
    }
    // get interface name
#ifdef _WIN32
    // strip off the "::ffff."
    ifname = maybe_status->at("result")["services"][endpoint]["ifaddr"];
    const auto pos = ifname.find("/");
    if (pos != std::string::npos)
    {
      ifname = ifname.substr(0, pos);
    }
#else
    ifname = maybe_status->at("result")["services"][endpoint]["ifname"];
#endif
  }
  catch (std::exception& ex)
  {
    std::cout << "failed to parse result: " << ex.what() << std::endl;
    return 1;
  }
  if (goUp)
  {
    std::optional<nlohmann::json> maybe_result;
    if (token.has_value())
    {
      maybe_result = LMQ_Request(
          lmq,
          connID,
          "llarp.exit",
          nlohmann::json{{"exit", exitAddress}, {"range", "0.0.0.0/0"}, {"token", *token}});
    }
    else
    {
      maybe_result = LMQ_Request(
          lmq, connID, "llarp.exit", nlohmann::json{{"exit", exitAddress}, {"range", "0.0.0.0/0"}});
    }

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

    const auto gateways = GetGatewaysNotOnInterface(ifname);
    if (gateways.empty())
    {
      std::cout << "cannot determine default gateway" << std::endl;
      return 1;
    }
    const auto ourGateway = gateways[0];
    for (const auto& ip : firstHops)
    {
      AddRoute(ip, ourGateway);
    }
    AddDefaultRouteViaInterface(ifname);
  }
  if (goDown)
  {
    DelDefaultRouteViaInterface(ifname);
    const auto gateways = GetGatewaysNotOnInterface(ifname);
    if (gateways.empty())
    {
      std::cout << "cannot determine default gateway" << std::endl;
      return 1;
    }
    const auto ourGateway = gateways[0];
    for (const auto& ip : firstHops)
    {
      DelRoute(ip, ourGateway);
    }
    LMQ_Request(lmq, connID, "llarp.exit", nlohmann::json{{"range", "0.0.0.0/0"}, {"unmap", true}});
  }

  return 0;
}

void
AddRoute(std::string ip, std::string gateway)
{
  std::stringstream ss;
#ifdef __linux__
  ss << "ip route add " << ip << "/32 via " << gateway;
#elif _WIN32
  ss << "route ADD " << ip << " MASK 255.255.255.255 METRIC 2 " << gateway;
#elif __APPLE__
  ss << "route -n add -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
  const auto cmd_str = ss.str();
  system(cmd_str.c_str());
}

void
DelRoute(std::string ip, std::string gateway)
{
  std::stringstream ss;
#ifdef __linux__
  ss << "ip route del " << ip << "/32 via " << gateway;
#elif _WIN32
  ss << "route DELETE " << ip << " MASK 255.255.255.255 METRIC 2" << gateway;
#elif __APPLE__
  ss << "route -n delete -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
  const auto cmd_str = ss.str();
  system(cmd_str.c_str());
}

void
AddDefaultRouteViaInterface(std::string ifname)
{
  std::stringstream ss;
#ifdef __linux__
  ss << "ip route add default dev " << ifname;
#elif _WIN32
  ss << "route ADD 0.0.0.0 MASK 0.0.0.0 " << ifname;
#elif __APPLE__
  ss << "route -n add -net 0.0.0.0 " << ifname << " 0.0.0.0";
#else
#error unsupported platform
#endif
  const auto cmd_str = ss.str();
  system(cmd_str.c_str());
}

void
DelDefaultRouteViaInterface(std::string ifname)
{
  std::stringstream ss;
#ifdef __linux__
  ss << "ip route del default dev " << ifname;
#elif _WIN32
  ss << "route DELETE 0.0.0.0 MASK 0.0.0.0 " << ifname;
#elif __APPLE__
  ss << "route -n delete -net 0.0.0.0 " << ifname << " 0.0.0.0";
#else
#error unsupported platform
#endif
  const auto cmd_str = ss.str();
  system(cmd_str.c_str());
}

std::vector<std::string>
GetGatewaysNotOnInterface(std::string ifname)
{
  std::vector<std::string> gateways;
#ifdef __linux__
  FILE* p = popen("ip route", "r");
  if (p == nullptr)
    return gateways;
  char* line = nullptr;
  size_t len = 0;
  ssize_t read = 0;
  while ((read = getline(&line, &len, p)) != -1)
  {
    std::string line_str(line, len);
    std::vector<std::string> words;
    std::istringstream instr(line_str);
    for (std::string word; std::getline(instr, word, ' ');)
    {
      words.emplace_back(std::move(word));
    }
    if (words[0] == "default" and words[1] == "via" and words[3] == "dev" and words[4] != ifname)
    {
      gateways.emplace_back(std::move(words[2]));
    }
  }
  pclose(p);
  return gateways;
#elif _WIN32
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
  PMIB_IPFORWARDTABLE pIpForwardTable;
  DWORD dwSize = 0;
  DWORD dwRetVal = 0;

  pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(sizeof(MIB_IPFORWARDTABLE));
  if (pIpForwardTable == nullptr)
    return gateways;

  if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
  {
    FREE(pIpForwardTable);
    pIpForwardTable = (MIB_IPFORWARDTABLE*)MALLOC(dwSize);
    if (pIpForwardTable == nullptr)
    {
      return gateways;
    }
  }

  if ((dwRetVal = GetIpForwardTable(pIpForwardTable, &dwSize, 0)) == NO_ERROR)
  {
    for (int i = 0; i < (int)pIpForwardTable->dwNumEntries; i++)
    {
      struct in_addr gateway, interface_addr;
      gateway.S_un.S_addr = (u_long)pIpForwardTable->table[i].dwForwardDest;
      interface_addr.S_un.S_addr = (u_long)pIpForwardTable->table[i].dwForwardNextHop;
      std::array<char, 128> interface_str{};
      strcpy_s(interface_str.data(), interface_str.size(), inet_ntoa(interface_addr));
      std::string interface_name{interface_str.data()};
      if ((!gateway.S_un.S_addr) and interface_name != ifname)
      {
        gateways.push_back(std::move(interface_name));
      }
    }
  }
  FREE(pIpForwardTable);
#undef MALLOC
#undef FREE
  return gateways;
#elif __APPLE__
  const auto maybe = llarp::GetIFAddr(ifname);
  if (not maybe.has_value())
    return gateways;
  const auto interface = maybe->toString();
  // mac os is so godawful man
  FILE* p = popen("netstat -rn -f inet", "r");
  if (p == nullptr)
    return gateways;
  char* line = nullptr;
  size_t len = 0;
  ssize_t read = 0;
  while ((read = getline(&line, &len, p)) != -1)
  {
    std::string line_str(line, len);
    if (line_str.find("default") == 0)
    {
      line_str = line_str.substr(7);
      while (line_str[0] == ' ')
      {
        line_str = line_str.substr(1);
      }
      const auto pos = line_str.find(" ");
      if (pos != std::string::npos)
      {
        auto gateway = line_str.substr(0, pos);
        if (gateway != interface)
          gateways.emplace_back(std::move(gateway));
      }
    }
  }
  pclose(p);
  return gateways;
#else
#error unsupported platform
#endif
}

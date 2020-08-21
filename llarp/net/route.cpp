#include "route.hpp"

namespace llarp::net
{
#ifndef __linux__
  void
  Execute(std::string cmd)
  {
    std::cout << cmd << std::endl;
#ifdef _WIN32
    system(cmd.c_str());
#else
    std::vector<std::string> parts_str;
    std::vector<const char*> parts_raw;
    std::stringstream in(cmd);
    for (std::string part; std::getline(in, part, ' ');)
    {
      if (part.empty())
        continue;
      parts_str.push_back(part);
    }
    for (const auto& part : parts_str)
    {
      parts_raw.push_back(part.c_str());
    }
    parts_raw.push_back(nullptr);
    const auto pid = fork();
    if (pid == -1)
    {
      throw std::runtime_error("failed to fork");
    }
    else if (pid == 0)
    {
      char* const* args = (char* const*)parts_raw.data();
      const auto result = execv(parts_raw[0], args);
      if (result)
      {
        std::cout << "failed: " << result << std::endl;
      }
      else
      {
        std::cout << "ok" << std::endl;
      }
      exit(result);
    }
    else
    {
      waitpid(pid, 0, 0);
    }
#endif
  }
#endif

  void
  AddRoute(std::string ip, std::string gateway)
  {
#ifdef __linux__
#else
    std::stringstream ss;
#if _WIN32
    ss << "route ADD " << ip << " MASK 255.255.255.255 " << gateway << " METRIC 2";
#elif __APPLE__
    ss << "route -n add -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
    Execute(ss.str());
#endif
  }

  void
  DelRoute(std::string ip, std::string gateway)
  {
#ifdef __linux__
#else
    std::stringstream ss;

#if _WIN32
    ss << "route DELETE " << ip << " MASK 255.255.255.255 " << gateway << " METRIC 2";
#elif __APPLE__
    ss << "route -n delete -host " << ip << " " << gateway;
#else
#error unsupported platform
#endif
    Execute(ss.str());
#endif
  }

  void
  AddDefaultRouteViaInterface(std::string ifname)
  {
#ifdef __linux__
    // Execute("/sbin/ip route add default dev " + ifname);
#elif _WIN32
    ifname.back()++;
    Execute("route ADD 0.0.0.0 MASK 128.0.0.0 " + ifname);
    Execute("route ADD 128.0.0.0 MASK 128.0.0.0 " + ifname);
#elif __APPLE__
    Execute("route -cloning add -net 0.0.0.0 -netmask 0.0.0.0 -interface " + ifname);
#else
#error unsupported platform
#endif
  }

  void
  DelDefaultRouteViaInterface(std::string ifname)
  {
#ifdef __linux__
    // Execute("/sbin/ip route del default dev " + ifname);
#elif _WIN32
    ifname.back()++;
    Execute("route DELETE 0.0.0.0 MASK 128.0.0.0 " + ifname);
    Execute("route DELETE 128.0.0.0 MASK 128.0.0.0 " + ifname);
#elif __APPLE__
    Execute("route -cloning delete -net 0.0.0.0 -netmask 0.0.0.0 -interface " + ifname);
#else
#error unsupported platform
#endif
  }

  std::vector<std::string>
  GetGatewaysNotOnInterface(std::string ifname)
  {
    std::vector<std::string> gateways;
#ifdef __linux__
    /*
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
    */
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
        StringCchCopy(interface_str.data(), interface_str.size(), inet_ntoa(interface_addr));
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

}  // namespace llarp::net

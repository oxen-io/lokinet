#include "common.hpp"
#include "config/config.hpp"

#include <netinet/in.h>

namespace llarp
{
  void
  in_addr_set(in_addr* addr, const char* str)
  {
    inet_aton(str, addr);
  }

  void
  Config_Init(py::module& mod)
  {
    using Config_ptr = std::shared_ptr<Config>;
    py::class_<Config, Config_ptr>(mod, "Config")
        .def(py::init<>())
        .def_readwrite("router", &Config::router)
        .def_readwrite("network", &Config::network)
        .def_readwrite("connect", &Config::connect)
        .def_readwrite("links", &Config::links)
        .def_readwrite("api", &Config::api)
        .def_readwrite("lokid", &Config::lokid)
        .def_readwrite("bootstrap", &Config::bootstrap)
        .def_readwrite("logging", &Config::logging)
        .def("LoadFile", &Config::Load)
        .def("LoadDefault", [](Config& self, bool isRelay, std::string dir) {
          return self.LoadDefault(isRelay, dir);
        });

    py::class_<RouterConfig>(mod, "RouterConfig")
        .def(py::init<>())
        .def_readwrite("minConnectedRouters", &RouterConfig::m_minConnectedRouters)
        .def_readwrite("maxConnectedRouters", &RouterConfig::m_maxConnectedRouters)
        .def_readwrite("netid", &RouterConfig::m_netId)
        .def_readwrite("nickname", &RouterConfig::m_nickname)
        .def_property(
            "dataDir",
            [](RouterConfig& self) { return self.m_dataDir.c_str(); },
            [](RouterConfig& self, std::string dir) { self.m_dataDir = dir; })
        .def_readwrite("blockBogons", &RouterConfig::m_blockBogons)
        .def_readwrite("publicOverride", &RouterConfig::m_publicOverride)
        .def_readwrite("ip4addr", &RouterConfig::m_ip4addr)
        .def(
            "overrideAddress",
            [](RouterConfig& self, std::string ip, std::string port) {
              llarp::Addr addr(ip);
              self.m_addrInfo.ip = *addr.addr6();

              int portInt = stoi(port);
              self.m_ip4addr.sin_port = portInt;
              self.m_addrInfo.port = portInt;

              self.m_publicOverride = true;
            })
        .def_readwrite("workerThreads", &RouterConfig::m_workerThreads)
        .def_readwrite("numNetThreads", &RouterConfig::m_numNetThreads)
        .def_readwrite("JobQueueSize", &RouterConfig::m_JobQueueSize);

    py::class_<NetworkConfig>(mod, "NetworkConfig")
        .def(py::init<>())
        .def_readwrite("enableProfiling", &NetworkConfig::m_enableProfiling)
        .def_readwrite("routerProfilesFile", &NetworkConfig::m_routerProfilesFile)
        .def_readwrite("endpointType", &NetworkConfig::m_endpointType)
        .def_readwrite("keyfile", &NetworkConfig::m_keyfile)
        .def_readwrite("endpointType", &NetworkConfig::m_endpointType)
        .def_readwrite("reachable", &NetworkConfig::m_reachable)
        .def_readwrite("hops", &NetworkConfig::m_hops)
        .def_readwrite("paths", &NetworkConfig::m_paths)
        .def_readwrite("snodeBlacklist", &NetworkConfig::m_snodeBlacklist)
        .def_readwrite("mapAddr", &NetworkConfig::m_mapAddr)
        .def_readwrite("strictConnect", &NetworkConfig::m_strictConnect);

    py::class_<ConnectConfig>(mod, "ConnectConfig")
        .def(py::init<>())
        .def_readwrite("routers", &ConnectConfig::routers);

    py::class_<DnsConfig>(mod, "DnsConfig").def(py::init<>());

    py::class_<LinksConfig>(mod, "LinksConfig")
        .def(py::init<>())
        .def(
            "setOutboundLink",
            [](LinksConfig& self, std::string interface, int family, uint16_t port) {
              LinksConfig::LinkInfo info;
              info.interface = std::move(interface);
              info.addressFamily = family;
              info.port = port;
              self.m_OutboundLink = std::move(info);
            })
        .def(
            "addInboundLink",
            [](LinksConfig& self, std::string interface, int family, uint16_t port) {
              LinksConfig::LinkInfo info;
              info.interface = std::move(interface);
              info.addressFamily = family;
              info.port = port;
              self.m_InboundLinks.push_back(info);
            });

    py::class_<ApiConfig>(mod, "ApiConfig")
        .def(py::init<>())
        .def_readwrite("enableRPCServer", &ApiConfig::m_enableRPCServer)
        .def_readwrite("rpcBindAddr", &ApiConfig::m_rpcBindAddr);

    py::class_<LokidConfig>(mod, "LokidConfig")
        .def(py::init<>())
        .def_readwrite("usingSNSeed", &LokidConfig::usingSNSeed)
        .def_readwrite("whitelistRouters", &LokidConfig::whitelistRouters)
        .def_readwrite("ident_keyfile", &LokidConfig::ident_keyfile)
        .def_readwrite("lokidRPCAddr", &LokidConfig::lokidRPCAddr)
        .def_readwrite("lokidRPCUser", &LokidConfig::lokidRPCUser)
        .def_readwrite("lokidRPCPassword", &LokidConfig::lokidRPCPassword);

    py::class_<BootstrapConfig>(mod, "BootstrapConfig")
        .def(py::init<>())
        .def_readwrite("routers", &BootstrapConfig::routers);

    py::class_<LoggingConfig>(mod, "LoggingConfig")
        .def(py::init<>())
        .def_readwrite("m_logType", &LoggingConfig::m_logType)
        .def_readwrite("m_logFile", &LoggingConfig::m_logFile);

    py::class_<sockaddr_in>(mod, "sockaddr_in")
        .def_readwrite("sin_family", &sockaddr_in::sin_family)
        .def_readwrite("sin_port", &sockaddr_in::sin_port)
        .def_readwrite("sin_addr", &sockaddr_in::sin_addr);

    py::class_<in_addr>(mod, "in_addr").def("set", &in_addr_set);
  }
}  // namespace llarp

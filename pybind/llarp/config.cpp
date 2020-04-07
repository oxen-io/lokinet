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
        .def_readwrite("netdb", &Config::netdb)
        .def_readwrite("dns", &Config::dns)
        .def_readwrite("links", &Config::links)
        .def_readwrite("services", &Config::services)
        .def_readwrite("system", &Config::system)
        .def_readwrite("api", &Config::api)
        .def_readwrite("lokid", &Config::lokid)
        .def_readwrite("bootstrap", &Config::bootstrap)
        .def_readwrite("logging", &Config::logging)
        .def("LoadFile", &Config::Load);

    py::class_<RouterConfig>(mod, "RouterConfig")
        .def(py::init<>())
        .def_readwrite("minConnectedRouters", &RouterConfig::m_minConnectedRouters)
        .def_readwrite("maxConnectedRouters", &RouterConfig::m_maxConnectedRouters)
        .def_readwrite("netid", &RouterConfig::m_netId)
        .def_readwrite("nickname", &RouterConfig::m_nickname)
        .def_readwrite("encryptionKeyfile", &RouterConfig::m_encryptionKeyfile)
        .def_readwrite("ourRcFile", &RouterConfig::m_ourRcFile)
        .def_readwrite("transportKeyfile", &RouterConfig::m_transportKeyfile)
        .def_readwrite("identKeyfile", &RouterConfig::m_identKeyfile)
        .def_readwrite("blockBogons", &RouterConfig::m_blockBogons)
        .def_readwrite("publicOverride", &RouterConfig::m_publicOverride)
        .def_readwrite("ip4addr", &RouterConfig::m_ip4addr)
        .def(
            "overrideAddress",
            [](RouterConfig& self, std::string ip, std::string port) {
              self.fromSection("public-ip", ip);
              self.fromSection("public-port", port);
            })
        .def_readwrite("workerThreads", &RouterConfig::m_workerThreads)
        .def_readwrite("numNetThreads", &RouterConfig::m_numNetThreads)
        .def_readwrite("JobQueueSize", &RouterConfig::m_JobQueueSize)
        .def_readwrite("DefaultLinkProto", &RouterConfig::m_DefaultLinkProto);

    py::class_<NetworkConfig>(mod, "NetworkConfig")
        .def(py::init<>())
        .def_readwrite("enableProfiling", &NetworkConfig::m_enableProfiling)
        .def_readwrite("routerProfilesFile", &NetworkConfig::m_routerProfilesFile)
        .def_readwrite("strictConnect", &NetworkConfig::m_strictConnect)
        .def_readwrite("netConfig", &NetworkConfig::m_netConfig);

    py::class_<ConnectConfig>(mod, "ConnectConfig")
        .def(py::init<>())
        .def_readwrite("routers", &ConnectConfig::routers);

    py::class_<NetdbConfig>(mod, "NetdbConfig")
        .def(py::init<>())
        .def_readwrite("nodedbDir", &NetdbConfig::m_nodedbDir);

    py::class_<DnsConfig>(mod, "DnsConfig")
        .def(py::init<>())
        .def_readwrite("netConfig", &DnsConfig::netConfig);

    py::class_<LinksConfig>(mod, "LinksConfig")
        .def(py::init<>())
        .def_readwrite("OutboundLink", &LinksConfig::m_OutboundLink)
        .def_readwrite("InboundLinks", &LinksConfig::m_InboundLinks);

    py::class_<ServicesConfig>(mod, "ServicesConfig")
        .def(py::init<>())
        .def_readwrite("services", &ServicesConfig::services);

    py::class_<SystemConfig>(mod, "SystemConfig")
        .def(py::init<>())
        .def_readwrite("pidfile", &SystemConfig::pidfile);

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
        .def_readwrite("LogJSON", &LoggingConfig::m_LogJSON)
        .def_readwrite("LogFile", &LoggingConfig::m_LogFile);

    py::class_<sockaddr_in>(mod, "sockaddr_in")
        .def_readwrite("sin_family", &sockaddr_in::sin_family)
        .def_readwrite("sin_port", &sockaddr_in::sin_port)
        .def_readwrite("sin_addr", &sockaddr_in::sin_addr);

    py::class_<in_addr>(mod, "in_addr").def("set", &in_addr_set);
  }
}  // namespace llarp

#include <stdexcept>
#include "common.hpp"

PYBIND11_MODULE(pylokinet, m)
{
  lokinet::Init_Context(m);
  lokinet::Init_Socket(m);
  m.def("libversion", &::lokinet_version_str);
  m.def("set_net_id", &::lokinet_set_netid);
  m.def("set_log_level", [](std::string level) {
    if (::lokinet_log_level(level.c_str()))
      throw std::invalid_argument{"bad log level"};
  });
}

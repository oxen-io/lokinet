#include "common.hpp"
#include "config/config.hpp"
namespace llarp
{
  void
  Config_Init(py::module & mod)
  {
    using Config_ptr = std::shared_ptr<Config>;
    py::class_<Config, Config_ptr>(mod, "Config")
    .def(py::init<>())
    .def("LoadFile", &Config::Load);
  }
}
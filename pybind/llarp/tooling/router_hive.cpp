#include "common.hpp"
#include <tooling/router_hive.hpp>

namespace tooling
{
  void
  RouterHive_Init(py::module& mod)
  {
    using RouterHive_ptr = std::shared_ptr< RouterHive >;
    py::class_< RouterHive, RouterHive_ptr >(mod, "RouterHive")
        .def(py::init<>())
        .def("StartAll", &RouterHive::StartRouters)
        .def("StopAll", &RouterHive::StopRouters);
             
  }
}
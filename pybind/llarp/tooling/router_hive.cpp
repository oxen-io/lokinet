#include "common.hpp"
#include <tooling/router_hive.hpp>
#include "llarp.hpp"
#include "pybind11/iostream.h"
namespace tooling
{
  void
  RouterHive_Init(py::module& mod)
  {
    using RouterHive_ptr = std::shared_ptr< RouterHive >;
    py::class_< RouterHive, RouterHive_ptr >(mod, "RouterHive")
        .def(py::init<>())
        .def("AddRouter", &RouterHive::AddRouter)
        .def("StartAll", &RouterHive::StartRouters)
        .def("StopAll", &RouterHive::StopRouters)
        .def("ForEachRouter", &RouterHive::ForEachRouter)
        .def("VisitRouter", &RouterHive::VisitRouter)
        .def("GetNextEvent", &RouterHive::GetNextEvent);
             
  }
}

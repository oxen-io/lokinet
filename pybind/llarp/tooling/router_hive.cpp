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
        .def("AddRelay", &RouterHive::AddRelay)
        .def("AddClient", &RouterHive::AddClient)
        .def("StartRelays", &RouterHive::StartRelays)
        .def("StartClients", &RouterHive::StartClients)
        .def("StopAll", &RouterHive::StopRouters)
        .def("ForEachRelay", &RouterHive::ForEachRelay)
        .def("ForEachClient", &RouterHive::ForEachClient)
        .def("ForEachRouter", &RouterHive::ForEachRouter)
        .def("GetNextEvent", &RouterHive::GetNextEvent);
             
  }
}

#include "common.hpp"
#include "pybind11/stl.h"
#include "pybind11/iostream.h"

#include <tooling/router_hive.hpp>
#include "router/abstractrouter.hpp"
#include "llarp.hpp"

namespace tooling
{
  void
  RouterHive_Init(py::module& mod)
  {
    using RouterHive_ptr = std::shared_ptr<RouterHive>;
    py::class_<RouterHive, RouterHive_ptr>(mod, "RouterHive")
        .def(py::init<>())
        .def("AddRelay", &RouterHive::AddRelay)
        .def("AddClient", &RouterHive::AddClient)
        .def("StartRelays", &RouterHive::StartRelays)
        .def("StartClients", &RouterHive::StartClients)
        .def("StopAll", &RouterHive::StopRouters)
        .def("ForEachRelayContext", &RouterHive::ForEachRelayContext)
        .def("ForEachClientContext", &RouterHive::ForEachClientContext)
        .def("ForEachRouterContext", &RouterHive::ForEachRouterContext)
        .def("ForEachRelayRouter", &RouterHive::ForEachRelayRouter)
        .def("ForEachClientRouter", &RouterHive::ForEachClientRouter)
        .def("ForEachRouterRouter", &RouterHive::ForEachRouterRouter)
        .def("GetNextEvent", &RouterHive::GetNextEvent)
        .def("GetAllEvents", &RouterHive::GetAllEvents)
        .def("RelayConnectedRelays", &RouterHive::RelayConnectedRelays)
        .def("GetRelayRCs", &RouterHive::GetRelayRCs)
        .def("GetRelay", &RouterHive::GetRelay);
  }
}  // namespace tooling

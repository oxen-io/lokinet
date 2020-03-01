#include "common.hpp"

#include "tooling/router_event.hpp"

#include <path/path.hpp>

namespace tooling
{
  void
  RouterEvent_Init(py::module & mod)
  {
    py::class_<RouterEvent>(mod, "RouterEvent")
    .def("__repr__", &RouterEvent::ToString)
    .def("__str__", &RouterEvent::ToString)
    .def_readonly("routerID", &RouterEvent::routerID)
    .def_readonly("triggered", &RouterEvent::triggered);

    py::class_<PathAttemptEvent, RouterEvent>(mod, "PathAttemptEvent")
    .def_readonly("hops", &PathAttemptEvent::hops);

    py::class_<PathRequestReceivedEvent, RouterEvent>(mod, "PathRequestReceivedEvent")
    .def_readonly("prevHop", &PathRequestReceivedEvent::prevHop)
    .def_readonly("nextHop", &PathRequestReceivedEvent::nextHop)
    .def_readonly("isEndpoint", &PathRequestReceivedEvent::isEndpoint);
  }

} // namespace tooling

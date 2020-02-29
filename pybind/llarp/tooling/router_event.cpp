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

    py::class_<PathBuildAttemptEvent, RouterEvent>(mod, "PathBuildAttemptEvent")
    .def_readonly("hops", &PathBuildAttemptEvent::hops);
  }

} // namespace tooling

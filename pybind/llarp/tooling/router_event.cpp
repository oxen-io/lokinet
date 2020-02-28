#include "common.hpp"

#include "tooling/router_event.hpp"

namespace tooling
{
  void
  RouterEvent_Init(py::module & mod)
  {
    py::class_<RouterEvent>(mod, "RouterEvent")
    .def(py::init<>())
    .def("ToString", &RouterEvent::ToString)
    .def_readonly("routerID", &RouterEvent::routerID);

    py::class_<PathBuildAttemptEvent>(mod, "PathBuildAttemptEvent")
    .def(py::init<>());
    //.def_readonly("hops", &PathBuildAttemptEvent::hops);
  }

} // namespace tooling

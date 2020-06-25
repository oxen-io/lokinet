#include "common.hpp"

#include "router/abstractrouter.hpp"

namespace llarp
{
  void
  AbstractRouter_Init(py::module& mod)
  {
    py::class_<AbstractRouter>(mod, "AbstractRouter")
        .def("rc", &AbstractRouter::rc)
        .def("Stop", &AbstractRouter::Stop)
        .def("peerDb", &AbstractRouter::peerDb);
  }
}  // namespace llarp

#include <common.hpp>

#include <llarp/router/abstractrouter.hpp>
#include <llarp/tooling/hive_router.hpp>

namespace llarp
{
  void
  AbstractRouter_Init(py::module& mod)
  {
    py::class_<AbstractRouter, std::shared_ptr<AbstractRouter>>(mod, "AbstractRouter")
        .def("rc", &AbstractRouter::rc)
        .def("Stop", &AbstractRouter::Stop)
        .def("peerDb", &AbstractRouter::peerDb);
  }

}  // namespace llarp

namespace tooling
{
  void
  HiveRouter_Init(py::module& mod)
  {
    py::class_<HiveRouter, llarp::AbstractRouter, std::shared_ptr<HiveRouter>>(mod, "HiveRouter")
        .def("disableGossiping", &HiveRouter::disableGossiping)
        .def("enableGossiping", &HiveRouter::enableGossiping);
  }
}  // namespace tooling

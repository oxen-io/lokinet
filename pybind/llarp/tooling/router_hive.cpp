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
        .def("StartAll", [](RouterHive_ptr self) {
          self->StartRouters([](llarp_main * ctx) {
            py::scoped_ostream_redirect stream_0(
              std::cout,                          
              py::module::import("sys").attr("stdout")
            );
            py::scoped_ostream_redirect stream_1(
              std::cerr,
              py::module::import("sys").attr("stderr") 
            );
            llarp_main_run(ctx, llarp_main_runtime_opts{false, false, false});
          });
        })
        .def("StopAll", &RouterHive::StopRouters)
        .def("ForEachRouter", &RouterHive::ForEachRouter)
        .def("VisitRouter", &RouterHive::VisitRouter)
        .def("GetNextEvent", &RouterHive::GetNextEvent);
             
  }
}

#include "common.hpp"
#include <llarp.hpp>

namespace llarp
{
  void
  Context_Init(py::module& mod)
  {
    using Context_ptr = std::shared_ptr< Context >;
    py::class_< Context, Context_ptr >(mod, "Context")
        .def(py::init< simulate::Sim_ptr >())
        .def("Setup",
             [](Context_ptr self) -> bool { return self->Setup() == 0; })
        .def("Run",
             [](Context_ptr self) -> int {
               return self->Run(llarp_main_runtime_opts{});
             })
        .def("IsUp", &Context::IsUp)
        .def("LooksAlive", &Context::LooksAlive)
        .def("Configure", &Context::Configure)
        .def("CallSafe", &Context::CallSafe);
  }
}  // namespace llarp

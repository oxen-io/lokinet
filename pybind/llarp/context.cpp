#include "common.hpp"
#include <llarp.hpp>
#include <router/router.cpp>
namespace llarp
{
  void
  Context_Init(py::module& mod)
  {
    using Context_ptr = std::shared_ptr< Context >;
    py::class_< Context, Context_ptr >(mod, "Context")
        .def("Setup",
             [](Context_ptr self) -> bool { return self->Setup() == 0; })
        .def("Run",
             [](Context_ptr self) -> int {
               return self->Run(llarp_main_runtime_opts{});
             })
        .def("IsUp", &Context::IsUp)
        .def("LooksAlive", &Context::LooksAlive)
        .def("Configure", &Context::Configure)
        .def("TrySendPacket", [](Context_ptr self, service::Address to, std::vector<byte_t> pkt) {
          auto ep = self->router->hiddenServiceContext().GetDefault();
          return ep->SendToServiceOrQueue(to, pkt, service::eProtocolControl);
        })
        .def("CallSafe", &Context::CallSafe);
  }
}  // namespace llarp

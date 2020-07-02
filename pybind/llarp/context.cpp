#include "common.hpp"
#include <llarp.hpp>
#include <tooling/hive_context.hpp>
#include <router/router.cpp>
#include "llarp/handlers/pyhandler.hpp"
namespace llarp
{
  void
  Context_Init(py::module& mod)
  {
    using Context_ptr = std::shared_ptr<Context>;
    py::class_<Context, Context_ptr>(mod, "Context")
        .def(
            "Setup",
            [](Context_ptr self, bool isRouter) {
              self->Setup({false, false, isRouter});
            })
        .def("Run", [](Context_ptr self) -> int { return self->Run(RuntimeOptions{}); })
        .def("Stop", [](Context_ptr self) { self->CloseAsync(); })
        .def("IsUp", &Context::IsUp)
        .def("IsRelay", [](Context_ptr self) -> bool { return self->router->IsServiceNode(); })
        .def("LooksAlive", &Context::LooksAlive)
        .def("Configure", &Context::Configure)
        .def(
            "TrySendPacket",
            [](Context_ptr self, std::string from, service::Address to, std::string pkt) {
              auto ep = self->router->hiddenServiceContext().GetEndpointByName(from);
              std::vector<byte_t> buf;
              buf.resize(pkt.size());
              std::copy_n(pkt.c_str(), pkt.size(), buf.data());
              return ep and ep->SendToServiceOrQueue(to, std::move(buf), service::eProtocolControl);
            })
        .def(
            "AddEndpoint",
            [](Context_ptr self, handlers::PythonEndpoint_ptr ep) {
              self->router->hiddenServiceContext().InjectEndpoint(ep->OurName, ep);
            })
        .def("CallSafe", &Context::CallSafe);
  }

}  // namespace llarp

namespace tooling
{
  void
  HiveContext_Init(py::module& mod)
  {
    py::class_<tooling::HiveContext>(mod, "HiveContext")
        .def("getRouterAsHiveRouter", &tooling::HiveContext::getRouterAsHiveRouter);
  }
}  // namespace tooling

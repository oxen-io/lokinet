#include <router_contact.hpp>
#include "common.hpp"

namespace llarp
{
  void
  RouterContact_Init(py::module& mod)
  {
    py::class_< RouterContact >(mod, "RouterContact")
        .def(py::init<>())
        .def_property_readonly("routerID", [](const RouterContact* const rc) -> llarp::RouterID {
            return llarp::RouterID(rc->pubkey);
        })
        .def("ReadFile", &RouterContact::Read)
        .def("WriteFile", &RouterContact::Write)
        .def("ToString", [](const RouterContact* const rc) -> std::string {
          return rc->ToJson().dump();
        })
        .def("Verify", [](const RouterContact* const rc) -> bool {
          const llarp_time_t now = llarp::time_now_ms();
          return rc->Verify(now);
        });
  }
}  // namespace llarp

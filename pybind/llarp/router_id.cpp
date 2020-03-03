#include "common.hpp"

#include "router_id.hpp"

namespace llarp
{
  void
  RouterID_Init(py::module & mod)
  {
    py::class_<RouterID>(mod, "RouterID")
        .def("FromHex",
             [](RouterID* r, const std::string& hex) -> bool {
               return HexDecode(hex.c_str(), r->data(), r->size());
             })
        .def("__repr__", &RouterID::ToString)
        .def("__str__", &RouterID::ToString)
        .def("ShortString", &RouterID::ShortString)
        .def("__eq__", [](const RouterID* const lhs, const RouterID* const rhs) {
            return *lhs == *rhs;
        });
  }
}

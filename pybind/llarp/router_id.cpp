#include <common.hpp>

#include <llarp/router_id.hpp>

namespace llarp
{
  void
  RouterID_Init(py::module& mod)
  {
    py::class_<RouterID>(mod, "RouterID")
        .def(
            "FromHex",
            [](RouterID* r, const std::string& hex) {
              if (hex.size() != 2 * r->size() || !oxenmq::is_hex(hex))
                throw std::runtime_error("FromHex requires a 64-digit hex string");
              oxenmq::from_hex(hex.begin(), hex.end(), r->data());
            })
        .def("__repr__", &RouterID::ToString)
        .def("__str__", &RouterID::ToString)
        .def("ShortString", &RouterID::ShortString)
        .def("__eq__", [](const RouterID& lhs, const RouterID& rhs) { return lhs == rhs; });
  }
}  // namespace llarp

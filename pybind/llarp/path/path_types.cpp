#include <path/path.hpp>
#include "common.hpp"

namespace llarp
{
  void
  PathTypes_Init(py::module & mod)
  {
    py::class_< PathID_t >(mod, "PathID")
    .def("__eq__", [](const PathID_t* const lhs, const PathID_t* const rhs) {
        return *lhs == *rhs;
    })
    .def("ShortHex", &PathID_t::ShortHex)
    .def("__str__", &PathID_t::ShortHex)
    .def("__repr__", &PathID_t::ShortHex);
  }

}  // namespace llarp

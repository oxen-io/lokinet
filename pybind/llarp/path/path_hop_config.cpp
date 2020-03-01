#include <path/path.hpp>
#include "common.hpp"

namespace llarp
{
  namespace path
  {
    void
    PathHopConfig_Init(py::module& mod)
    {
      auto str_func = [](PathHopConfig *hop) {
          std::string s = "Hop: routerID = ";
          s += RouterID(hop->rc.pubkey).ToString();
          s += ", next routerID = ";
          s += hop->upstream.ToString();
          return s;
          };
      py::class_< PathHopConfig >(mod, "PathHopConfig")
      .def_readonly("rc", &PathHopConfig::rc)
      .def_readonly("upstreamRouter", &PathHopConfig::upstream)
      .def("ToString", str_func)
      .def("__str__", str_func)
      .def("__repr__", str_func);
    }
  } // namespace llarp::path
}  // namespace llarp

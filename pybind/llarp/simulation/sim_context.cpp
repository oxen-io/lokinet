#include "common.hpp"
#include <llarp.hpp>

namespace llarp
{
  namespace simulate
  {
    void
    SimContext_Init(py::module& mod)
    {
      py::class_< Simulation, Sim_ptr >(mod, "Simulation")
          .def(py::init<>())
          .def("AddNode", &Simulation::AddNode)
          .def("DelNode", &Simulation::DelNode);
      py::object context  = py::cast(std::make_shared< Simulation >());
      mod.attr("context") = context;
    }

  }  // namespace simulate
}  // namespace llarp

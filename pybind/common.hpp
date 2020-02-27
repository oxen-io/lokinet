#pragma once
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace llarp
{
  void
  Context_Init(py::module &mod);

  void
  CryptoTypes_Init(py::module &mod);

  void
  RouterContact_Init(py::module &mod);

  void
  Config_Init(py::module & mod);

}  // namespace llarp

namespace tooling
{
  void
  RouterHive_Init(py::module & mod);
}
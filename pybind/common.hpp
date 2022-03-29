#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <unordered_map>
#include <lokinet.h>

namespace py = pybind11;

namespace lokinet
{
  void
  Init_Context(py::module& mod);

  void
  Init_Socket(py::module& mod);

}  // namespace lokinet

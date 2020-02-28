#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/*
namespace pybind11
{
namespace detail
{
template<typename T>
struct type_caster<nonstd::optional<T>> 
: public optional_caster<nonstd::optional<T>> {};

template <typename CharT, class Traits>
struct type_caster<simple_string_view>
: string_caster<simple_string_view, true> {};

} // namespace pybind11::detail
} // namespace pybind11
*/

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

  void
  RouterEvent_Init(py::module & mod);
}

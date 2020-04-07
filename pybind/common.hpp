#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <nonstd/optional.hpp>

#include <unordered_map>

namespace py = pybind11;

namespace pybind11
{
  namespace detail
  {
    template < typename Key, typename Value, typename Hash, typename Equal,
               typename Alloc >
    struct type_caster<
        std::unordered_multimap< Key, Value, Hash, Equal, Alloc > >
        : map_caster< std::unordered_multimap< Key, Value, Hash, Equal, Alloc >,
                      Key, Value >
    {
    };

    template < typename T >
    struct type_caster< nonstd::optional< T > >
        : public optional_caster< nonstd::optional< T > >
    {
    };

    /*
    template <typename CharT, class Traits>
    struct type_caster<simple_string_view>
    : string_caster<simple_string_view, true> {};
    */

  }  // namespace detail
}  // namespace pybind11

namespace llarp
{
  void
  Context_Init(py::module &mod);

  void
  CryptoTypes_Init(py::module &mod);

  void
  RouterID_Init(py::module &mod);

  void
  RouterContact_Init(py::module &mod);

  void
  Config_Init(py::module &mod);

  void
  PathTypes_Init(py::module &mod);

  namespace dht
  {
    void
    DHTTypes_Init(py::module &mod);
  }

  namespace path
  {
    void
    PathHopConfig_Init(py::module &mod);
  }

  namespace handlers
  {
    void
    PyHandler_Init(py::module &mod);
  }

  namespace service
  {
    void
    Address_Init(py::module &mod);
  }

}  // namespace llarp

namespace tooling
{
  void
  RouterHive_Init(py::module &mod);

  void
  RouterEvent_Init(py::module &mod);
}  // namespace tooling

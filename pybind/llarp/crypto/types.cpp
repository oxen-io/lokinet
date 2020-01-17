#include <crypto/types.hpp>
#include "common.hpp"

namespace llarp
{
  void
  CryptoTypes_Init(py::module& mod)
  {
    py::class_< PubKey >(mod, "PubKey")
        .def(py::init<>())
        .def("FromHex", &PubKey::FromString)
        .def("__repr__", &PubKey::ToString);
    py::class_< RouterID >(mod, "RouterID")
        .def(py::init<>())
        .def("FromHex",
             [](RouterID* r, const std::string& hex) -> bool {
               return HexDecode(hex.c_str(), r->data(), r->size());
             })
        .def("__repr__", &RouterID::ToString);
    py::class_< SecretKey >(mod, "SecretKey")
        .def(py::init<>())
        .def("LoadFile", &SecretKey::LoadFromFile)
        .def("SaveFile", &SecretKey::SaveToFile)
        .def("ToPublic", &SecretKey::toPublic);
    py::class_< Signature >(mod, "Signature")
        .def(py::init<>())
        .def("__repr__", [](const Signature sig) -> std::string {
          std::stringstream ss;
          ss << sig;
          return ss.str();
        });
  }

}  // namespace llarp

#include "common.hpp"

#include "tooling/router_event.hpp"
#include "tooling/dht_event.hpp"

#include <path/path.hpp>

namespace tooling
{
  void
  RouterEvent_Init(py::module & mod)
  {
    py::class_<RouterEvent>(mod, "RouterEvent")
    .def("__repr__", &RouterEvent::ToString)
    .def("__str__", &RouterEvent::ToString)
    .def_readonly("routerID", &RouterEvent::routerID)
    .def_readonly("triggered", &RouterEvent::triggered);

    py::class_<PathAttemptEvent, RouterEvent>(mod, "PathAttemptEvent")
    .def_readonly("hops", &PathAttemptEvent::hops);

    py::class_<PathRequestReceivedEvent, RouterEvent>(mod, "PathRequestReceivedEvent")
    .def_readonly("prevHop", &PathRequestReceivedEvent::prevHop)
    .def_readonly("nextHop", &PathRequestReceivedEvent::nextHop)
    .def_readonly("isEndpoint", &PathRequestReceivedEvent::isEndpoint);
    py::class_<PubIntroReceivedEvent, RouterEvent>(mod, "DhtPubIntroReceievedEvent")
    .def_readonly("from", &PubIntroReceivedEvent::From)
    .def_readonly("location", &PubIntroReceivedEvent::IntrosetLocation)
      .def_readonly("relayOrder", &PubIntroReceivedEvent::RelayOrder)
      .def_readonly("txid", &PubIntroReceivedEvent::TxID);
    py::class_<GotIntroReceivedEvent, RouterEvent>(mod, "DhtGotIntroReceievedEvent")
    .def_readonly("from", &GotIntroReceivedEvent::From)
    .def_readonly("location", &GotIntroReceivedEvent::Introset)
      .def_readonly("relayOrder", &GotIntroReceivedEvent::RelayOrder)
      .def_readonly("txid", &GotIntroReceivedEvent::TxID);
  }

} // namespace tooling

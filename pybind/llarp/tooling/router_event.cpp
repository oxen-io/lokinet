#include "common.hpp"
#include "pybind11/stl.h"

#include "tooling/router_event.hpp"
#include "tooling/dht_event.hpp"
#include "tooling/path_event.hpp"
#include "tooling/rc_event.hpp"

#include <messages/relay_status.hpp>
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
    .def_readonly("txid", &PathRequestReceivedEvent::txid)
    .def_readonly("rxid", &PathRequestReceivedEvent::rxid)
    .def_readonly("isEndpoint", &PathRequestReceivedEvent::isEndpoint);

    py::class_<PathStatusReceivedEvent, RouterEvent>(mod, "PathStatusReceivedEvent")
    .def_readonly("rxid", &PathStatusReceivedEvent::rxid)
    .def_readonly("status", &PathStatusReceivedEvent::rxid)
    .def_property_readonly("Successful", [](const PathStatusReceivedEvent* const ev) {
        return ev->status == llarp::LR_StatusRecord::SUCCESS;
    });

    py::class_<PubIntroReceivedEvent, RouterEvent>(mod, "DhtPubIntroReceivedEvent")
    .def_readonly("from", &PubIntroReceivedEvent::From)
    .def_readonly("location", &PubIntroReceivedEvent::IntrosetLocation)
      .def_readonly("relayOrder", &PubIntroReceivedEvent::RelayOrder)
      .def_readonly("txid", &PubIntroReceivedEvent::TxID);

    py::class_<GotIntroReceivedEvent, RouterEvent>(mod, "DhtGotIntroReceivedEvent")
    .def_readonly("from", &GotIntroReceivedEvent::From)
    .def_readonly("location", &GotIntroReceivedEvent::Introset)
      .def_readonly("relayOrder", &GotIntroReceivedEvent::RelayOrder)
      .def_readonly("txid", &GotIntroReceivedEvent::TxID);

    py::class_<RCGossipReceivedEvent, RouterEvent>(mod, "RCGossipReceivedEvent")
    .def_readonly("rc", &RCGossipReceivedEvent::rc)
    .def("LongString", &RCGossipReceivedEvent::LongString);
  }

} // namespace tooling

#include "common.hpp"

PYBIND11_MODULE(pyllarp, m)
{
  tooling::RouterHive_Init(m);
  tooling::RouterEvent_Init(m);
  llarp::RouterContact_Init(m);
  llarp::CryptoTypes_Init(m);
  llarp::Context_Init(m);
  llarp::Config_Init(m);
  llarp::handlers::PyHandler_Init(m);
  llarp::service::Address_Init(m);
}

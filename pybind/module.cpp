
#include "common.hpp"

PYBIND11_MODULE(pyllarp, m)
{
  llarp::simulate::SimContext_Init(m);
  llarp::RouterContact_Init(m);
  llarp::CryptoTypes_Init(m);
  llarp::Context_Init(m);
}


#include "common.hpp"

PYBIND11_MODULE(pyllarp, m)
{
  llarp::simulate::SimContext_Init(m);
  llarp::RouterContact_Init(m);
}

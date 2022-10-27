#include <llarp/util/service_manager.hpp>

namespace llarp::sys
{
  // we will have this implemented on android when there is time to
  NOP_SystemLayerHandler _manager{};
  I_SystemLayerManager* const service_manager = &_manager;
}  // namespace llarp::sys

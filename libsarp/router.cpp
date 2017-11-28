#include <sarp/router.h>
#include <sarp/link.h>
#include "link.hpp"
#include <vector>

namespace sarp
{
  struct Router
  {
    std::vector<Link> activeLinks;
  };
}


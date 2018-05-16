#include <llarp/link.h>

bool llarp_link_initialized(struct llarp_link * link)
{
  return link && link->impl && link->name && link->configure && link->start_link && link->stop_link && link->iter_sessions && link->try_establish && link->free_impl;
}
  

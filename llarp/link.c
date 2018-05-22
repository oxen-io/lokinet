#include <llarp/link.h>

bool
llarp_link_initialized(struct llarp_link* link)
{
  return link && link->impl && link->name && link->get_our_address
      && link->configure && link->start_link && link->stop_link
      && link->iter_sessions && link->try_establish && link->mark_session_active
      && link->free_impl;
}

bool
llarp_link_session_initialized(struct llarp_link_session* s)
{
  return s && s->impl && s->sendto && s->timeout && s->close;
}

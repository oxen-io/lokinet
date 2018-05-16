#include <llarp/iwp.h>
#include <llarp/ev.h>

struct iwp_link
{
  struct llarp_alloc * alloc;
  struct llarp_ev_loop *netloop;
  const char * keyfile;
  struct llarp_udp_io udp;
};


static const char * iwp_link_name()
{
  return "IWP";
}

static bool iwp_link_configure(struct llarp_link * l, const char * ifname, int af, uint16_t port)
{
  struct iwp_link * link = l->impl;
  link->udp.user = link;
  return llarp_ev_add_udp(link->netloop, &link->udp) == 0;
}

static struct iwp_link * iwp_link_alloc(struct iwp_configure_args * args)
{
  struct iwp_link * l = args->mem->alloc(args->mem, sizeof(struct iwp_link), 16);
  l->alloc = args->mem;
  l->netloop = args->ev;
  l->keyfile = args->keyfile;
  return l;
}

void iwp_link_init(struct llarp_link * link, struct iwp_configure_args args,
                   struct llarp_msg_muxer * muxer)
{
  link->impl = iwp_link_alloc(&args);
  link->name = iwp_link_name;
  link->configure = iwp_link_configure;
}

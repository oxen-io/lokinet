#include <llarp/iwp.h>
#include <cstring>

namespace iwp
{

  struct link_impl
  {
    link_impl(llarp_seckey_t k)
    {
      memcpy(seckey, k, sizeof(seckey));
    }
    
    llarp_seckey_t seckey;

    int configure(const char * ifname, int af, uint16_t port)
    {
      // todo: implement
      return -1;
    }
  };

  static int configure_addr(struct llarp_link * l, const char * ifname, int af, uint16_t port)
  {
    link_impl * link = static_cast<link_impl *>(l->impl);
    return link->configure(ifname, af, port);
  }

}

extern "C" {

void iwp_link_init(struct llarp_link * link, struct iwp_configure_args args, struct llarp_msg_muxer * muxer)
{
  link->impl = new iwp::link_impl(args.seckey);
  link->configure_addr = &iwp::configure_addr;
}

}

#include <llarp/iwp.h>
#include <cstring>
#include <list>
#include <string>

namespace iwp {

struct link_impl {
  link_impl(llarp_seckey_t k, llarp_msg_muxer *m) : muxer(m) {
    memcpy(seckey, k, sizeof(seckey));
  }

  std::list<llarp_link_ev_listener> link_listeners;

  llarp_seckey_t seckey;

  llarp_msg_muxer *muxer;

  std::string linkname;

  bool configure(const char *ifname, int af, uint16_t port) {
    // todo: implement
    linkname = std::string(ifname) + std::string("+") + std::to_string(port);
    return false;
  }

  const char *name() { return linkname.c_str(); }

  bool start(llarp_logic *logic) {
    // todo: implement
    return false;
  }

  bool stop() {
    // todo: implement
    return false;
  }
};

static bool configure(struct llarp_link *l, const char *ifname, int af,
                      uint16_t port) {
  link_impl *link = static_cast<link_impl *>(l->impl);
  return link->configure(ifname, af, port);
}

static bool start_link(struct llarp_link *l, struct llarp_logic *logic) {
  link_impl *link = static_cast<link_impl *>(l->impl);
  return link->start(logic);
}

static bool stop_link(struct llarp_link *l) {
  link_impl *link = static_cast<link_impl *>(l->impl);
  return link->stop();
}

static void free_link(struct llarp_link *l) {
  link_impl *link = static_cast<link_impl *>(l->impl);
  delete link;
}

static const char *link_name(struct llarp_link *l) {
  link_impl *link = static_cast<link_impl *>(l->impl);
  return link->name();
}

}  // namespace iwp

extern "C" {

bool iwp_link_init(struct llarp_link *link, struct iwp_configure_args args,
                   struct llarp_msg_muxer *muxer) {
  llarp_seckey_t seckey;
  args.crypto->keygen(&seckey);
  link->impl = new iwp::link_impl(seckey, muxer);

  link->name = &iwp::link_name;
  link->configure = &iwp::configure;
  link->start_link = &iwp::start_link;
  link->stop_link = &iwp::stop_link;
  link->free = &iwp::free_link;
  return true;
}
}

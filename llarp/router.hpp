#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP
#include <llarp/link.h>
#include <llarp/router.h>
#include <llarp/router_contact.h>
#include <functional>
#include <map>

#include "mem.hpp"
#include "fs.hpp"

namespace llarp {
struct router_links {
  llarp_link *link = nullptr;
  router_links *next = nullptr;
};

}  // namespace llarp

struct llarp_router {
  bool ready;
  // transient iwp encryption key
  fs::path transport_keyfile = "transport.key";

  // nodes to connect to on startup
  std::map<std::string, fs::path> connect;

  // long term identity key
  fs::path ident_keyfile = "identity.key";
  
  // path to write our self signed rc to
  fs::path our_rc_file = "rc.signed";


  llarp_rc rc;

  llarp_ev_loop * netloop;
  llarp_threadpool *tp;
  llarp_logic * logic;
  llarp::router_links links;
  llarp_crypto crypto;
  llarp_msg_muxer muxer;
  llarp_path_context *paths;
  llarp_alloc * mem;
  llarp_seckey_t identity;

  llarp_router(llarp_alloc * mem);
  ~llarp_router();

  void AddLink(struct llarp_link *link);

  void ForEachLink(std::function<void(llarp_link *)> visitor);

  void Close();

  bool Ready();

  bool EnsureIdentity();
  bool SaveRC();

  uint8_t * pubkey() { return llarp_seckey_topublic(identity); }
};

#endif

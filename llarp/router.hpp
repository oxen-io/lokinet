#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP
#include <llarp/link.h>
#include <llarp/router.h>
#include <llarp/router_contact.h>
#include <functional>
#include <list>
#include <map>

#include "fs.hpp"
#include "mem.hpp"

namespace llarp
{
  struct try_connect_ctx
  {
    llarp_router *router = nullptr;
    llarp_ai addr;
  };

}  // namespace llarp

struct llarp_router
{
  bool ready;
  // transient iwp encryption key
  fs::path transport_keyfile = "transport.key";

  // nodes to connect to on startup
  std::map< std::string, fs::path > connect;

  // long term identity key
  fs::path ident_keyfile = "identity.key";

  // path to write our self signed rc to
  fs::path our_rc_file = "rc.signed";

  llarp_rc rc;

  llarp_ev_loop *netloop;
  llarp_threadpool *tp;
  llarp_logic *logic;
  llarp_crypto crypto;
  llarp_msg_muxer muxer;
  llarp_path_context *paths;
  llarp_alloc *mem;
  llarp_seckey_t identity;

  std::list< llarp_link * > links;
  std::map< char, llarp_frame_handler > frame_handlers;

  llarp_router(llarp_alloc *mem);
  ~llarp_router();

  void
  AddLink(struct llarp_link *link);

  void
  Close();

  bool
  Ready();

  void
  Run();

  bool
  EnsureIdentity();

  bool
  SaveRC();

  uint8_t *
  pubkey()
  {
    return llarp_seckey_topublic(identity);
  }

  void
  try_connect(fs::path rcfile);

  bool
  has_session_to(const uint8_t *pubkey) const;

  static bool
  iter_try_connect(llarp_router_link_iter *i, llarp_router *router,
                   llarp_link *l);

  static void
  on_try_connect_result(llarp_link_establish_job *job);
};

#endif

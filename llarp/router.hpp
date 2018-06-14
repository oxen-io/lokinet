#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP
#include <llarp/dht.h>
#include <llarp/link.h>
#include <llarp/nodedb.h>
#include <llarp/router.h>
#include <llarp/router_contact.h>
#include <llarp/path.hpp>

#include <functional>
#include <list>
#include <map>
#include <unordered_map>

#include <llarp/dht.hpp>
#include <llarp/link_message.hpp>

#include "crypto.hpp"
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

  fs::path encryption_keyfile = "encryption.key";

  // path to write our self signed rc to
  fs::path our_rc_file = "rc.signed";

  // our router contact
  llarp_rc rc;

  llarp_ev_loop *netloop;
  llarp_threadpool *tp;
  llarp_logic *logic;
  llarp_crypto crypto;
  llarp::PathContext paths;
  llarp::SecretKey identity;
  llarp::SecretKey encryption;
  llarp_threadpool *disk;
  llarp_dht_context *dht = nullptr;

  llarp_nodedb *nodedb;

  // buffer for serializing link messages
  byte_t linkmsg_buffer[MAX_LINK_MSG_SIZE];

  // should we be sending padded messages every interval?
  bool sendPadding = false;

  uint32_t ticker_job_id = 0;

  llarp::InboundMessageParser inbound_msg_parser;

  llarp_link *outboundLink = nullptr;
  std::list< llarp_link * > inboundLinks;

  typedef std::queue< llarp::ILinkMessage * > MessageQueue;

  /// outbound message queue
  std::unordered_map< llarp::PubKey, MessageQueue, llarp::PubKeyHash >
      outboundMesssageQueue;

  /// loki verified routers
  std::unordered_map< llarp::PubKey, llarp_rc, llarp::PubKeyHash > validRouters;

  llarp_router();
  ~llarp_router();

  bool
  HandleRecvLinkMessage(struct llarp_link_session *from, llarp_buffer_t msg);

  void
  AddInboundLink(struct llarp_link *link);

  bool
  InitOutboundLink();

  /// initialize us as a service node
  void
  InitServiceNode();

  void
  Close();

  bool
  Ready();

  void
  Run();

  static void
  ConnectAll(void *user, uint64_t orig, uint64_t left);

  bool
  EnsureIdentity();

  bool
  EnsureEncryptionKey();

  bool
  SaveRC();

  const byte_t *
  pubkey() const
  {
    return llarp::seckey_topublic(identity);
  }

  void
  try_connect(fs::path rcfile);

  /// send to remote router or queue for sending
  /// returns false on overflow
  /// returns true on successful queue
  bool
  SendToOrQueue(const llarp::RouterID &remote,
                std::vector< llarp::ILinkMessage * > msgs);

  /// sendto or drop
  void
  SendTo(llarp::RouterID remote, llarp::ILinkMessage *msg);

  /// manually flush outbound message queue for just 1 router
  void
  FlushOutboundFor(const llarp::RouterID &remote, llarp_link *chosen);

  /// manually discard all pending messages to remote router
  void
  DiscardOutboundFor(const llarp::RouterID &remote);

  /// flush outbound message queue
  void
  FlushOutbound();

  /// called by link when a remote session is expunged
  void
  SessionClosed(const llarp::RouterID &remote);

  /// call internal router ticker
  void
  Tick();

  /// schedule ticker to call i ms from now
  void
  ScheduleTicker(uint64_t i = 1000);

  void
  async_verify_RC(llarp_link_session *session, bool isExpectingClient,
                  llarp_link_establish_job *job = nullptr);

  static bool
  iter_try_connect(llarp_router_link_iter *i, llarp_router *router,
                   llarp_link *l);

  static void
  on_try_connect_result(llarp_link_establish_job *job);

  static void
  connect_job_retry(void *user);

  static void
  on_verify_client_rc(llarp_async_verify_rc *context);

  static void
  on_verify_server_rc(llarp_async_verify_rc *context);

  static void
  handle_router_ticker(void *user, uint64_t orig, uint64_t left);

  static bool
  send_padded_message(struct llarp_link_session_iter *itr,
                      struct llarp_link_session *peer);

  static void
  HandleAsyncLoadRCForSendTo(llarp_async_load_rc *async);
};

#endif

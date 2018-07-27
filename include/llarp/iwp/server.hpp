#pragma once
#include <llarp/iwp.h>
#include <llarp/threading.hpp>
#include "llarp/iwp/establish_job.hpp"
#include "router.hpp"
#include "session.hpp"

#include <algorithm>
#include <fstream>

struct llarp_link
{
  typedef std::mutex mtx_t;
  typedef std::unique_lock< mtx_t > lock_t;
  /*
  typedef llarp::util::DummyMutex mtx_t;
  typedef llarp::util::DummyLock lock_t;
  */

  llarp_router *router;
  llarp_crypto *crypto;
  llarp_logic *logic;
  llarp_ev_loop *netloop;
  llarp_async_iwp *iwp;
  llarp_threadpool *worker;
  llarp_link *parent = nullptr;
  llarp_udp_io udp;
  llarp::Addr addr;
  char keyfile[255];
  uint32_t timeout_job_id;

  const char *
  name() const
  {
    return m_name;
  }

  const char *m_name;

  typedef std::unordered_map< llarp::Addr, llarp_link_session *,
                              llarp::Addr::Hash >
      LinkMap_t;

  LinkMap_t m_sessions;
  mtx_t m_sessions_Mutex;

  typedef std::unordered_map< llarp::PubKey, llarp::Addr, llarp::PubKey::Hash >
      SessionMap_t;

  SessionMap_t m_Connected;
  mtx_t m_Connected_Mutex;
  std::atomic< bool > pumpingLogic;

  typedef std::unordered_map< llarp::Addr, llarp_link_session *,
                              llarp::Addr::Hash >
      PendingSessionMap_t;
  PendingSessionMap_t m_PendingSessions;
  mtx_t m_PendingSessions_Mutex;

  llarp::SecretKey seckey;

  llarp_link(const llarp_iwp_args &args);

  ~llarp_link();

  bool
  has_intro_from(const llarp::Addr &from);

  void
  put_intro_from(llarp_link_session *s);

  void
  remove_intro_from(const llarp::Addr &from);

  // set that src address has identity pubkey
  void
  MapAddr(const llarp::Addr &src, const llarp::PubKey &identity);

  bool
  has_session_to(const byte_t *pubkey);

  void
  TickSessions();

  bool
  sendto(const byte_t *pubkey, llarp_buffer_t buf);

  void
  UnmapAddr(const llarp::Addr &src);

  llarp_link_session *
  create_session(const llarp::Addr &src);

  bool
  has_session_via(const llarp::Addr &dst);

  llarp_link_session *
  find_session(const llarp::Addr &addr);

  void
  put_session(const llarp::Addr &src, llarp_link_session *impl);

  void
  clear_sessions();

  /// safe iterate sessions
  void
  iterate_sessions(std::function< bool(llarp_link_session *) > visitor);

  static void
  handle_logic_pump(void *user);

  void
  PumpLogic();

  void
  RemoveSession(llarp_link_session *s);

  uint8_t *
  pubkey();

  bool
  ensure_privkey();

  bool
  keygen(const char *fname);

  static void
  handle_cleanup_timer(void *l, uint64_t orig, uint64_t left);

  // this is called in net threadpool
  static void
  handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                  const void *buf, ssize_t sz);

  void
  cancel_timer();

  void
  issue_cleanup_timer(uint64_t timeout);

  void
  get_our_address(struct llarp_ai *addr);

  static void
  after_recv(llarp_udp_io *udp);

  bool
  configure(struct llarp_ev_loop *netloop, const char *ifname, int af,
            uint16_t port);

  bool
  start_link(struct llarp_logic *pLogic);

  bool
  stop_link();

  bool
  try_establish(struct llarp_link_establish_job *job);
};

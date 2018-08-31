#pragma once
#include <llarp/iwp.hpp>
#include <llarp/threading.hpp>
#include "llarp/iwp/establish_job.hpp"
#include "router.hpp"
#include "session.hpp"

#ifndef __linux__
#ifdef saddr
#undef saddr
#endif
#endif

#include <algorithm>
#include <fstream>

struct llarp_link
{
  typedef llarp::util::Mutex mtx_t;
  typedef llarp::util::Lock lock_t;

  llarp_router *router;
  llarp_crypto *crypto;
  llarp_logic *logic;
  llarp_ev_loop *netloop;
  llarp_async_iwp *iwp;
  llarp_threadpool *worker;
  llarp_link *parent = nullptr;
  llarp_udp_io udp;
  llarp::Addr addr;
  std::string keyfile;
  uint32_t timeout_job_id;

  const char *
  name() const
  {
    return m_name;
  }

  const char *m_name;

  typedef std::unordered_map< llarp::Addr, llarp_link_session,
                              llarp::Addr::Hash >
      LinkMap_t;

  LinkMap_t m_sessions;
  mtx_t m_sessions_Mutex;

  typedef std::unordered_map< llarp::PubKey, llarp::Addr, llarp::PubKey::Hash >
      SessionMap_t;

  SessionMap_t m_Connected;
  mtx_t m_Connected_Mutex;
  std::atomic< bool > pumpingLogic;

  llarp::SecretKey seckey;

  llarp_link(const llarp_iwp_args &args);

  ~llarp_link();

  bool
  has_intro_from(const llarp::Addr &from);

  void
  remove_from(const llarp::Addr &from);

  /// does nothing if we have no session already established
  void
  KeepAliveSessionTo(const byte_t *pubkey);

  /// does nothing if we have no session already established
  void
  CloseSessionTo(const byte_t *pubkey);

  bool
  has_session_to(const byte_t *pubkey);

  void
  TickSessions();

  bool
  sendto(const byte_t *pubkey, llarp_buffer_t buf);

  bool
  has_session_via(const llarp::Addr &dst);

  void
  MapAddr(const llarp::Addr &addr, const llarp::PubKey &pk);

  void
  visit_session(const llarp::Addr &addr,
                std::function< void(llarp_link_session &) > visit);

  void
  pending_session_active(const llarp::Addr &addr);

  void
  clear_sessions();

  static void
  handle_logic_pump(void *user);

  void
  PumpLogic();

  const uint8_t *
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
  get_our_address(llarp::AddressInfo &addr);

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

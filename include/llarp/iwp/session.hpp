#pragma once

#include "codel.hpp"
#include "frame_state.hpp"
#include "llarp/buffer.h"
#include "llarp/crypto.hpp"
#include "llarp/crypto_async.h"
#include "llarp/router_contact.h"
#include "llarp/time.h"
#include "llarp/types.h"
#include "net.hpp"

struct llarp_udp_io;
struct llarp_async_iwp;
struct llarp_logic;
struct llarp_link;
struct transit_message;
struct llarp_link_establish_job;

struct llarp_link_session
{
  static constexpr llarp_time_t SESSION_TIMEOUT     = 10000;
  static constexpr llarp_time_t KEEP_ALIVE_INTERVAL = SESSION_TIMEOUT / 4;
  static constexpr size_t MAX_PAD                   = 128;

  llarp_link_session(llarp_udp_io *u, llarp_async_iwp *i, llarp_crypto *c,
                     llarp_logic *l, const byte_t *seckey, const llarp::Addr &a);

  ~llarp_link_session();

  void
  session_start();

  bool sendto(llarp_buffer_t);

  bool
  has_timed_out();
  bool
  timedout(llarp_time_t now, llarp_time_t timeout = SESSION_TIMEOUT);

  void
  close();
  void
  session_established();

  llarp_link *
  get_parent();
  llarp_rc *
  get_remote_router();

  bool
  CheckRCValid();
  bool
  IsEstablished();
  void
  send_LIM();
  bool
  is_invalidated() const;

  void
  done();
  void
  pump();
  void
  introduce(uint8_t *pub);

  // void handle_verify_intro(iwp_async_intro *intro);
  // void handle_verify_introack(iwp_async_introack *introack);
  void
  intro_ack();
  void
  on_intro_ack(const void *buf, size_t sz);
  void
  on_intro(const void *buf, size_t sz);
  void
  on_session_start(const void *buf, size_t sz);
  void
  encrypt_frame_async_send(const void *buf, size_t sz);

  // void send_keepalive(void *user);
  bool
  Tick(llarp_time_t now);
  void
  PumpCryptoOutbound();
  // void PumpCodelInbound();
  // void handle_crypto_outbound();
  // void handle_verify_session_start(iwp_async_session_start *s);
  // void handle_establish_timeout(void *user, uint64_t orig, uint64_t
  // left); void handle_introack_generated(iwp_async_introack *i);

  // this is called from net thread
  void
  recv(const void *buf, size_t sz);

  llarp_udp_io *udp;
  llarp_crypto *crypto;
  llarp_async_iwp *iwp;
  llarp_logic *logic;

  llarp_link_session *parent = nullptr;
  llarp_link *serv           = nullptr;

  llarp_rc *our_router = nullptr;
  llarp_rc remote_router;

  llarp::SecretKey eph_seckey;
  llarp::PubKey remote;
  llarp::SharedSecret sessionkey;

  llarp_link_establish_job *establish_job = nullptr;

  /// cached timestamp for frame creation
  llarp_time_t now;
  llarp_time_t lastKeepalive = 0;
  uint32_t establish_job_id  = 0;
  uint32_t frames            = 0;
  bool working               = false;

  llarp::util::CoDelQueue< iwp_async_frame *, FrameGetTime, FramePutTime >
      outboundFrames;
  /*
  std::mutex m_EncryptedFramesMutex;
  std::queue< iwp_async_frame > encryptedFrames;
  llarp::util::CoDelQueue< iwp_async_frame *, FrameGetTime, FramePutTime >
      decryptedFrames;
   */

  uint32_t pump_send_timer_id = 0;
  uint32_t pump_recv_timer_id = 0;

  llarp::Addr addr;
  iwp_async_intro intro;
  iwp_async_introack introack;
  iwp_async_session_start start;
  // frame_state frame;
  bool started_inbound_codel = false;

  byte_t token[32];
  byte_t workbuf[MAX_PAD + 128];

  enum State
  {
    eInitial,
    eIntroRecv,
    eIntroSent,
    eIntroAckSent,
    eIntroAckRecv,
    eSessionStartSent,
    eLIMSent,
    eEstablished,
    eTimeout
  };

  State state;
  void
  EnterState(State st);

  //  private:
  void
  add_outbound_message(uint64_t id, transit_message *msg);
  void
  EncryptOutboundFrames();
  iwp_async_frame *
  alloc_frame(const void *buf, size_t sz);
  void
  decrypt_frame(const void *buf, size_t sz);

  frame_state frame;
};

struct llarp_link_session_iter
{
  void *user;
  struct llarp_link *link;
  bool (*visit)(struct llarp_link_session_iter *, struct llarp_link_session *);
};
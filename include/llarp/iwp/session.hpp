#pragma once

#include <atomic>
#include <llarp/codel.hpp>
#include "frame_state.hpp"
#include "llarp/buffer.h"
#include "llarp/crypto.hpp"
#include "llarp/crypto_async.h"
#include "llarp/net.hpp"
#include "llarp/router_contact.h"
#include "llarp/time.h"
#include "llarp/types.h"

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

  llarp_link_session(llarp_link *l, const byte_t *seckey, const llarp::Addr &a);

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
  keepalive();

  void
  PumpCryptoOutbound();

  // process inbound and outbound queues (logic thread)
  void
  TickLogic(llarp_time_t now);

  // this is called from net thread
  void
  recv(const void *buf, size_t sz);

  llarp_router *
  Router();

  llarp_udp_io *udp    = nullptr;
  llarp_crypto *crypto = nullptr;
  llarp_async_iwp *iwp = nullptr;

  llarp_link *serv = nullptr;

  llarp_rc *our_router = nullptr;
  llarp_rc remote_router;

  llarp::SecretKey eph_seckey;
  llarp::PubKey remote;
  llarp::SharedSecret sessionkey;

  llarp_link_establish_job *establish_job = nullptr;

  llarp_time_t createdAt     = 0;
  llarp_time_t lastKeepalive = 0;
  uint32_t establish_job_id  = 0;
  uint32_t frames            = 0;
  std::atomic< bool > working;

  llarp::util::CoDelQueue< iwp_async_frame, FrameGetTime, FramePutTime,
                           FrameCompareTime >
      outboundFrames;

  llarp::util::CoDelQueue< iwp_async_frame, FrameGetTime, FramePutTime,
                           FrameCompareTime >
      decryptedFrames;

  llarp::Addr addr;

  /// timestamp last intro packet sent at
  llarp_time_t lastIntroSentAt = 0;
  uint32_t intro_resend_job_id = 0;

  iwp_async_session_start start;
  iwp_async_introack introack;

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

  void
  add_outbound_message(uint64_t id, transit_message *msg);
  void
  EncryptOutboundFrames();

  iwp_async_frame *
  alloc_frame(const void *buf, size_t sz);
  void
  decrypt_frame(const void *buf, size_t sz);

  static void
  handle_frame_decrypt(iwp_async_frame *f);

  static void
  handle_introack_timeout(void *user, uint64_t timeout, uint64_t left);

  frame_state frame;
};

struct llarp_link_session_iter
{
  void *user;
  struct llarp_link *link;
  bool (*visit)(struct llarp_link_session_iter *, struct llarp_link_session *);
};

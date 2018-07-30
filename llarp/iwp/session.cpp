#ifdef _MSC_VER
#define NOMINMAX
#endif
#include <llarp/iwp.h>
#include <llarp/crypto.hpp>
#include <llarp/iwp/server.hpp>
#include <llarp/iwp/session.hpp>
#include "address_info.hpp"
#include "buffer.hpp"
#include "link/encoder.hpp"
#include "llarp/ev.h"  // for handle_frame_encrypt
#include <algorithm>

static void
handle_crypto_outbound(void *u)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(u);
  self->EncryptOutboundFrames();
  self->working = false;
}

// TODO: move this orphan function?
static void
handle_frame_encrypt(iwp_async_frame *frame)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(frame->user);
  if(llarp_ev_udp_sendto(self->udp, self->addr, frame->buf, frame->sz) == -1)
    llarp::LogWarn("sendto failed");
}

llarp_link_session::llarp_link_session(llarp_link *l, const byte_t *seckey,
                                       const llarp::Addr &a)
    : udp(&l->udp)
    , crypto(&l->router->crypto)
    , iwp(l->iwp)
    , serv(l)
    , outboundFrames("iwp_outbound")
    , decryptedFrames("iwp_inbound")
    , addr(a)
    , state(eInitial)
    , frame(this)
{
  if(seckey)
    eph_seckey = seckey;
  else
    crypto->encryption_keygen(eph_seckey);
  llarp_rc_clear(&remote_router);
  crypto->randbytes(token, 32);
  frame.alive();
  working.store(false);
  createdAt = llarp_time_now_ms();
}

llarp_link_session::~llarp_link_session()
{
  llarp_rc_free(&remote_router);
  frame.clear();
}

llarp_router *
llarp_link_session::Router()
{
  return serv->router;
}

bool
llarp_link_session::sendto(llarp_buffer_t msg)
{
  auto now = llarp_time_now_ms();
  if(timedout(now))
    return false;
  auto id = ++frame.txids;
  // llarp::LogDebug("session sending to, number", id);
  llarp::ShortHash digest;
  crypto->shorthash(digest, msg);
  transit_message *m = new transit_message(msg, digest, id);
  add_outbound_message(id, m);
  return true;
}

bool
llarp_link_session::timedout(llarp_time_t now, llarp_time_t timeout)
{
  if(now <= frame.lastEvent)
    return false;
  auto diff = now - frame.lastEvent;
  return diff >= timeout;
}

bool
llarp_link_session::has_timed_out()
{
  auto now = llarp_time_now_ms();
  return timedout(now);
}

static void
send_keepalive(void *user)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(user);
  // if both sides agree on invalidation
  if(self->is_invalidated())
  {
    // don't send keepalive
    llarp::LogInfo("session cant send keepalive because were invalid");
    return;
  }
  // all zeros means keepalive
  byte_t tmp[8] = {0};
  // set flags for tx
  frame_header hdr(tmp);
  hdr.flags() = self->frame.txflags;

  // send frame after encrypting
  auto buf            = llarp::StackBuffer< decltype(tmp) >(tmp);
  self->lastKeepalive = llarp_time_now_ms();

  self->encrypt_frame_async_send(buf.base, buf.sz);
  self->pump();
  self->PumpCryptoOutbound();
}

void
llarp_link_session::close()
{
  // set our side invalidated and close async when the other side also marks
  // as session invalidated
  frame.txflags |= eSessionInvalidated;
  // TODO: add timer for session invalidation
  llarp_logic_queue_job(serv->logic, {this, &send_keepalive});
}

void
llarp_link_session::session_established()
{
  llarp::RouterID remote = remote_router.pubkey;
  llarp::LogInfo("Session to ", remote, " established");
  EnterState(eEstablished);
  serv->MapAddr(addr, remote_router.pubkey);
  llarp_logic_cancel_call(serv->logic, establish_job_id);
}

llarp_rc *
llarp_link_session::get_remote_router()
{
  return &remote_router;
}

void
llarp_link_session::add_outbound_message(uint64_t id, transit_message *msg)
{
  llarp::LogDebug("add outbound message ", id, " of size ",
                  msg->msginfo.totalsize(),
                  " numfrags=", (int)msg->msginfo.numfrags(),
                  " lastfrag=", (int)msg->msginfo.lastfrag());

  frame.queue_tx(id, msg);
  pump();
  PumpCryptoOutbound();
}

bool
llarp_link_session::CheckRCValid()
{
  // verify signatuire
  if(!llarp_rc_verify_sig(crypto, &remote_router))
    return false;

  auto &list = remote_router.addrs->list;
  if(list.size() == 0)  // the remote node is a client node so accept it
    return true;
  // check if the RC owns a pubkey that we are using
  for(auto &ai : list)
  {
    if(memcmp(ai.enc_key, remote, PUBKEYSIZE) == 0)
      return true;
  }
  return false;
}

bool
llarp_link_session::IsEstablished()
{
  return state == eEstablished;
}

void
llarp_link_session::send_LIM()
{
  llarp::LogDebug("send LIM");
  llarp::ShortHash digest;
  // 64 bytes overhead for link message
  byte_t tmp[MAX_RC_SIZE + 64];
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  // return a llarp_buffer_t of encoded link message
  if(llarp::EncodeLIM(&buf, our_router))
  {
    // rewind message buffer
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    // hash message buffer
    crypto->shorthash(digest, buf);
    auto id  = frame.txids++;
    auto msg = new transit_message(buf, digest, id);
    // put into outbound send queue
    add_outbound_message(id, msg);
    // enter state
    EnterState(eLIMSent);
  }
  else
    llarp::LogError("LIM Encode failed");
}

static void
handle_generated_session_start(iwp_async_session_start *start)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(start->user);

  if(llarp_ev_udp_sendto(link->udp, link->addr, start->buf, start->sz) == -1)
    llarp::LogError("sendto failed");
  link->EnterState(llarp_link_session::State::eSessionStartSent);
  link->working = false;
}

static void
handle_verify_intro(iwp_async_intro *intro)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(intro->user);
  if(!intro->buf)
  {
    self->serv->remove_intro_from(self->addr);
    llarp::LogError("intro verify failed from ", self->addr, " via ",
                    self->serv->addr);
    delete self;
    return;
  }
  self->intro_ack();
}

static void
handle_verify_introack(iwp_async_introack *introack)
{
  llarp_link_session *link =
      static_cast< llarp_link_session * >(introack->user);
  auto logic = link->serv->logic;

  link->working = false;
  if(introack->buf == nullptr)
  {
    // invalid signature
    llarp::LogError("introack verify failed from ", link->addr);
    return;
  }
  // cancel resend
  llarp_logic_cancel_call(logic, link->intro_resend_job_id);

  link->EnterState(llarp_link_session::eIntroAckRecv);
  link->session_start();
}

static void
handle_establish_timeout(void *user, uint64_t orig, uint64_t left)
{
  if(orig == 0)
    return;
  llarp_link_session *self = static_cast< llarp_link_session * >(user);
  self->establish_job_id   = 0;
  if(self->establish_job)
  {
    llarp_link_establish_job *job = self->establish_job;
    self->establish_job           = nullptr;
    job->link                     = self->serv;
    if(self->IsEstablished())
    {
      job->session = self;
    }
    else
    {
      // timer timeout
      job->session = nullptr;
    }
    job->result(job);
  }
}

void
llarp_link_session::done()
{
}

void
llarp_link_session::PumpCryptoOutbound()
{
  if(working)
    return;
  working = true;
  llarp_threadpool_queue_job(serv->worker, {this, &handle_crypto_outbound});
}

// void llarp_link_session::PumpCodelInbound()
// {
//   pump_recv_timer_id =
//       llarp_logic_call_later(logic,
//                              {decryptedFrames.nextTickInterval, this,
//                               &handle_inbound_codel_delayed});
// }

void
llarp_link_session::EnterState(State st)
{
  llarp::LogDebug("EnterState - entering state: ", st,
                  state == eLIMSent ? "eLIMSent" : "",
                  state == eSessionStartSent ? "eSessionStartSent" : "");
  frame.alive();
  state = st;
  if(state == eSessionStartSent || state == eIntroAckSent)
  {
    // llarp::LogInfo("EnterState - ",  state==eLIMSent?"eLIMSent":"",
    // state==eSessionStartSent?"eSessionStartSent":"");
    // PumpCodelInbound();
    // PumpCodelOutbound();
    PumpCryptoOutbound();
    // StartInboundCodel();
  }
}

void
llarp_link_session::on_intro(const void *buf, size_t sz)
{
  llarp::LogDebug("session onintro");
  if(sz >= sizeof(workbuf))
  {
    // too big?
    llarp::LogError("intro too big");
    delete this;
    return;
  }
  if(serv->has_intro_from(addr))
  {
    llarp::LogError("duplicate intro from ", addr);
    delete this;
    return;
  }
  serv->put_intro_from(this);
  // copy so we own it
  memcpy(workbuf, buf, sz);
  intro.buf = workbuf;
  intro.sz  = sz;
  // give secret key
  intro.secretkey = eph_seckey;
  // and nonce
  intro.nonce = intro.buf + 32;
  intro.user  = this;
  // set call back hook
  intro.hook = &handle_verify_intro;
  // put remote pubkey into this buffer
  intro.remote_pubkey = remote;

  // call
  EnterState(eIntroRecv);
  working = true;
  iwp_call_async_verify_intro(iwp, &intro);
}

void
llarp_link_session::on_intro_ack(const void *buf, size_t sz)
{
  if(sz >= sizeof(workbuf))
  {
    // too big?
    llarp::LogError("introack too big");
    return;
  }
  // copy buffer so we own it
  memcpy(workbuf, buf, sz);
  // set intro ack parameters
  introack.buf           = workbuf;
  introack.sz            = sz;
  introack.nonce         = workbuf + 32;
  introack.remote_pubkey = remote;
  introack.token         = token;
  introack.secretkey     = eph_seckey;
  introack.user          = this;
  introack.hook          = &handle_verify_introack;
  // async verify
  working = true;
  iwp_call_async_verify_introack(iwp, &introack);
}

bool
llarp_link_session::is_invalidated() const
{
  return frame.flags_agree(eSessionInvalidated);
}

llarp_link *
llarp_link_session::get_parent()
{
  return serv;
}

void
llarp_link_session::TickLogic(llarp_time_t now)
{
  std::queue< iwp_async_frame * > q;
  decryptedFrames.Process(q);
  while(q.size())
  {
    auto &front = q.front();
    handle_frame_decrypt(front);
    delete front;
    q.pop();
  }
  frame.process_inbound_queue();
  frame.retransmit(now);
  pump();
}

bool
llarp_link_session::Tick(llarp_time_t now)
{
  if(timedout(now, SESSION_TIMEOUT))
  {
    // we are timed out
    // when we are done doing stuff with all of our frames from the crypto
    // workers we are done
    llarp::LogWarn("Tick - ", addr, " timed out with ", frames,
                   " frames left, working=", working);
    return !working;
  }
  if(is_invalidated())
  {
    // both sides agreeed to session invalidation
    // terminate our session when all of our frames from the crypto workers
    // are done
    llarp::LogWarn("Tick - ", addr, " invaldiated session with ", frames,
                   " frames left");
    return !working;
  }
  if(state == eLIMSent || state == eEstablished)
  {
    if(now - lastKeepalive > KEEP_ALIVE_INTERVAL)
      send_keepalive(this);
  }
  return false;
}

void
llarp_link_session::EncryptOutboundFrames()
{
  std::queue< iwp_async_frame * > outq;
  outboundFrames.Process(outq);
  while(outq.size())
  {
    auto &front = outq.front();

    // if(iwp_encrypt_frame(&front))
    // q.push(front);
    if(iwp_encrypt_frame(front))
      handle_frame_encrypt(front);
    delete front;
    outq.pop();
  }
}

static void
handle_verify_session_start(iwp_async_session_start *s)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(s->user);
  self->serv->remove_intro_from(self->addr);
  if(!s->buf)
  {
    // verify fail
    // TODO: remove session?
    llarp::LogWarn("session start verify failed from ", self->addr);
    self->serv->RemoveSession(self);
    return;
  }
  self->send_LIM();
  self->working = false;
}

static void
handle_introack_generated(iwp_async_introack *i)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(i->user);

  if(i->buf && link->serv->has_intro_from(link->addr))
  {
    // track it with the server here
    if(link->serv->has_session_via(link->addr))
    {
      // duplicate session
      llarp::LogWarn("duplicate session to ", link->addr);
      link->working = false;
      return;
    }
    link->frame.alive();
    link->EnterState(llarp_link_session::State::eIntroAckSent);
    link->serv->put_session(link->addr, link);
    llarp::LogDebug("send introack to ", link->addr, " via ", link->serv->addr);
    llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
  }
  else
  {
    // failed to generate?
    llarp::LogWarn("failed to generate introack");
  }
  link->working = false;
}

static void
handle_generated_intro(iwp_async_intro *i)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(i->user);
  link->working            = false;
  if(i->buf)
  {
    llarp::LogInfo("send intro to ", link->addr);
    if(llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz) == -1)
    {
      llarp::LogWarn("send intro failed");
      return;
    }
    link->EnterState(llarp_link_session::eIntroSent);
    link->lastIntroSentAt     = llarp_time_now_ms();
    auto dlt                  = (link->createdAt - link->lastIntroSentAt) + 500;
    auto logic                = link->serv->logic;
    link->intro_resend_job_id = llarp_logic_call_later(
        logic, {dlt, link, &llarp_link_session::handle_introack_timeout});
  }
  else
  {
    llarp::LogWarn("failed to generate intro");
  }
}

void
llarp_link_session::handle_introack_timeout(void *user, uint64_t timeout,
                                            uint64_t left)
{
  if(timeout && left == 0)
  {
    // timeout reached
    llarp_link_session *self = static_cast< llarp_link_session * >(user);
    // retry introduce
    self->introduce(nullptr);
  }
}

void
llarp_link_session::introduce(uint8_t *pub)
{
  llarp::LogDebug("session introduce");
  if(pub)
    memcpy(remote, pub, PUBKEYSIZE);
  intro.buf   = workbuf;
  size_t w0sz = (llarp_randint() % MAX_PAD);
  intro.sz    = (32 * 3) + w0sz;
  // randomize w0
  if(w0sz)
  {
    crypto->randbytes(intro.buf + (32 * 3), w0sz);
  }

  intro.nonce     = intro.buf + 32;
  intro.secretkey = eph_seckey;
  // copy in pubkey
  intro.remote_pubkey = remote;
  // randomize nonce
  crypto->randbytes(intro.nonce, 32);
  // async generate intro packet
  intro.user = this;
  intro.hook = &handle_generated_intro;
  working    = true;
  iwp_call_async_gen_intro(iwp, &intro);
  // start introduce timer
  if(pub)
    establish_job_id = llarp_logic_call_later(
        serv->logic, {5000, this, &handle_establish_timeout});
}

void
llarp_link_session::handle_frame_decrypt(iwp_async_frame *frame)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(frame->user);
  if(frame->success)
  {
    if(self->frame.process(frame->buf + 64, frame->sz - 64))
    {
      self->frame.alive();
    }
    else
      llarp::LogError("invalid frame from ", self->addr);
  }
  else
    llarp::LogError("decrypt frame fail from ", self->addr);
}

void
llarp_link_session::decrypt_frame(const void *buf, size_t sz)
{
  if(sz > 64)
  {
    // auto frame = alloc_frame(inboundFrames, buf, sz);
    // inboundFrames.Put(frame);
    auto f = alloc_frame(buf, sz);

    if(iwp_decrypt_frame(f))
    {
      decryptedFrames.Put(f);
    }
    else
    {
      llarp::LogWarn("decrypt frame fail");
      delete f;
    }
    // f->hook = &handle_frame_decrypt;
    // iwp_call_async_frame_decrypt(iwp, f);
  }
  else
    llarp::LogWarn("short packet of ", sz, " bytes");
}

void
llarp_link_session::session_start()
{
  llarp::LogInfo("session gen start");
  size_t w2sz = llarp_randint() % MAX_PAD;
  start.buf   = workbuf;
  start.sz    = w2sz + (32 * 3);
  start.nonce = workbuf + 32;
  crypto->randbytes(start.nonce, 32);
  start.token = token;
  memcpy(start.buf + 64, token, 32);
  if(w2sz)
    crypto->randbytes(start.buf + (32 * 3), w2sz);
  start.remote_pubkey = remote;
  start.secretkey     = eph_seckey;
  start.sessionkey    = sessionkey;
  start.user          = this;
  start.hook          = &handle_generated_session_start;
  working             = true;
  iwp_call_async_gen_session_start(iwp, &start);
}

void
llarp_link_session::on_session_start(const void *buf, size_t sz)
{
  llarp::LogInfo("session start");
  if(sz > sizeof(workbuf))
  {
    llarp::LogDebug("session start too big");
    return;
  }
  // own the buffer
  memcpy(workbuf, buf, sz);
  // verify session start
  start.buf           = workbuf;
  start.sz            = sz;
  start.nonce         = workbuf + 32;
  start.token         = token;
  start.remote_pubkey = remote;
  start.secretkey     = eph_seckey;
  start.sessionkey    = sessionkey;
  start.user          = this;
  start.hook          = &handle_verify_session_start;
  working             = true;
  iwp_call_async_verify_session_start(iwp, &start);
}

void
llarp_link_session::intro_ack()
{
  if(serv->has_session_via(addr))
  {
    llarp::LogWarn("won't ack intro for duplicate session from ", addr);
    return;
  }
  llarp::LogDebug("session introack");
  uint16_t w1sz = llarp_randint() % MAX_PAD;
  introack.buf  = workbuf;
  introack.sz   = (32 * 3) + w1sz;
  // randomize padding
  if(w1sz)
    crypto->randbytes(introack.buf + (32 * 3), w1sz);

  // randomize nonce
  introack.nonce = introack.buf + 32;
  crypto->randbytes(introack.nonce, 32);
  // token
  introack.token = token;

  // keys
  introack.remote_pubkey = remote;
  introack.secretkey     = eph_seckey;

  // call
  introack.user = this;
  introack.hook = &handle_introack_generated;
  iwp_call_async_gen_introack(iwp, &introack);
}

// this is called from net thread
void
llarp_link_session::recv(const void *buf, size_t sz)
{
  // llarp::LogDebug("session recv", state);

  // frame_header hdr((byte_t *)buf);
  // llarp::LogDebug("recv - message header type ", (int)hdr.msgtype());

  switch(state)
  {
    case eInitial:
    case eIntroRecv:
      // got intro
      llarp::LogDebug("session recv - intro");
      on_intro(buf, sz);
      break;
    case eIntroSent:
      // got intro ack
      llarp::LogDebug("session recv - introack");
      on_intro_ack(buf, sz);
      break;
    case eIntroAckSent:
      // probably a session start
      llarp::LogDebug("session recv - sessionstart");
      on_session_start(buf, sz);
      break;

    case eSessionStartSent:
    case eLIMSent:
    case eEstablished:
      // session is started
      /*
      llarp::LogDebug("session recv - ",
                      state == eSessionStartSent ? "startsent" : "",
                      state == eLIMSent ? "limset" : "",
                      state == eEstablished ? "established" : "");
      */
      decrypt_frame(buf, sz);
      break;
    default:
      llarp::LogError("session recv - invalid state");
      // invalid state?
      break;
  }
}

// TODO: fix orphan
iwp_async_frame *
llarp_link_session::alloc_frame(const void *buf, size_t sz)
{
  // TODO don't hard code 1500
  if(sz > 1500)
  {
    llarp::LogWarn("alloc frame - frame too big, >1500");
    return nullptr;
  }

  iwp_async_frame *frame = new iwp_async_frame;
  if(buf)
    memcpy(frame->buf, buf, sz);
  frame->iwp        = iwp;
  frame->sz         = sz;
  frame->user       = this;
  frame->sessionkey = sessionkey;
  /// TODO: this could be rather slow
  // frame->created = now;
  // llarp::LogInfo("alloc_frame putting into q");
  // q.Put(frame);
  return frame;
}

void
llarp_link_session::encrypt_frame_async_send(const void *buf, size_t sz)
{
  // 64 bytes frame overhead for nonce and hmac
  iwp_async_frame *frame = alloc_frame(nullptr, sz + 64);
  memcpy(frame->buf + 64, buf, sz);
  // maybe add upto 128 random bytes to the packet
  auto padding = llarp_randint() % MAX_PAD;
  if(padding)
    crypto->randbytes(frame->buf + 64 + sz, padding);
  frame->sz += padding;
  // frame is modified, so now we can push it to queue
  outboundFrames.Put(frame);
}

void
llarp_link_session::pump()
{
  bool flush = false;
  llarp_buffer_t buf;
  std::queue< sendbuf_t * > q;
  frame.sendqueue.Process(q);
  while(q.size())
  {
    auto &front = q.front();
    buf         = front->Buffer();
    encrypt_frame_async_send(buf.base, buf.sz);
    delete front;
    q.pop();
    flush = true;
  }
  if(flush)
    PumpCryptoOutbound();
}

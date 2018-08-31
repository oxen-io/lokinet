#ifdef _MSC_VER
#define NOMINMAX
#endif
#include <algorithm>
#include <llarp/crypto.hpp>
#include <llarp/iwp.hpp>
#include <llarp/iwp/server.hpp>
#include <llarp/iwp/session.hpp>
#include "address_info.hpp"
#include "buffer.hpp"
#include "link/encoder.hpp"
#include "llarp/ev.h"  // for handle_frame_encrypt

static void
handle_crypto_outbound(void *u)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(u);
  self->EncryptOutboundFrames();
  self->working = false;
}

llarp_link_session::llarp_link_session()
    : outboundFrames("iwp_outbound")
    , decryptedFrames("iwp_inbound")
    , state(eInitial)
    , frame(this)
{
}

void
llarp_link_session::init(llarp_link *l, const byte_t *seckey,
                         const llarp::Addr &a)
{
  udp    = &l->udp;
  crypto = &l->router->crypto;
  iwp    = l->iwp;
  serv   = l;
  addr   = a;
  if(seckey)
    eph_seckey = seckey;
  else
    crypto->encryption_keygen(eph_seckey);
  crypto->randbytes(token, 32);
  frame.alive();
  working.store(false);
  createdAt = llarp_time_now_ms();
}

llarp_link_session::~llarp_link_session()
{
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
  auto id = frame.txids++;
  // llarp::LogDebug("session sending to, number", id);
  llarp::ShortHash digest;
  crypto->shorthash(digest, msg);
  add_outbound_message(id, digest, msg);
  return true;
}

bool
llarp_link_session::timedout(llarp_time_t now, llarp_time_t timeout)
{
  if(frame.lastEvent == 0)
    return false;
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
  if(self->is_invalidated())
  {
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
  keepalive();
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

llarp::RouterContact *
llarp_link_session::get_remote_router()
{
  return &remote_router;
}

void
llarp_link_session::add_outbound_message(uint64_t id,
                                         const llarp::ShortHash &digest,
                                         llarp_buffer_t buf)
{
  // insert and generate xmit
  frame.tx.insert(std::make_pair(id, transit_message(buf, digest, id)))
      .first->second.generate_xmit(frame.sendqueue, frame.txflags);
  pump();
  PumpCryptoOutbound();
}

bool
llarp_link_session::CheckRCValid()
{
  // verify signatuire
  if(!remote_router.VerifySignature(crypto))
    return false;

  if(remote_router.addrs.size()
     == 0)  // the remote node is a client node so accept it
    return true;
  // check if the RC owns a pubkey that we are using
  for(auto &ai : remote_router.addrs)
  {
    if(ai.pubkey == remote)
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
  llarp::ShortHash digest;
  // 64 bytes overhead for link message
  byte_t tmp[MAX_RC_SIZE + 64];
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  // return a llarp_buffer_t of encoded link message
  if(llarp::EncodeLIM(&buf, &serv->router->rc))
  {
    EnterState(eLIMSent);
    // rewind message buffer
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    // hash message buffer
    crypto->shorthash(digest, buf);
    // send
    add_outbound_message(0, digest, buf);
    ++frame.txids;
  }
  else
    llarp::LogError("LIM Encode failed");
}

static void
handle_generated_session_start(iwp_async_session_start *start)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(start->user);

  if(llarp_ev_udp_sendto(link->udp, link->addr, start->buf, start->sz) == -1)
  {
    llarp::LogError("sendto failed");
    return;
  }
  link->working = false;
  link->EnterState(llarp_link_session::eSessionStartSent);
}

static void
handle_verify_intro(iwp_async_intro *intro)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(intro->user);
  if(self == nullptr)
    return;
  self->working = false;
  if(!intro->buf)
  {
    return;
  }
  delete[] intro->buf;
  memcpy(self->remote, intro->remote_pubkey, 32);
  llarp::LogInfo("got intro from ", llarp::PubKey(self->remote));
  self->intro_ack();
}

static void
handle_verify_introack(iwp_async_introack *introack)
{
  llarp_link_session *link =
      static_cast< llarp_link_session * >(introack->user);

  if(introack->buf == nullptr)
  {
    // invalid signature
    link->working = false;
    llarp::LogError("introack verify failed from ", link->addr);
    return;
  }
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
    if(!self->working)
      job->result(job);
  }
}

void
llarp_link_session::done()
{
  if(intro_resend_job_id)
    llarp_logic_cancel_call(serv->logic, intro_resend_job_id);
  if(establish_job_id)
    llarp_logic_cancel_call(serv->logic, establish_job_id);
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
  if(sz < 32 * 3)
    return;
  // copy
  auto intro = new iwp_async_intro;
  intro->buf = new byte_t[sz];
  memcpy(intro->buf, buf, sz);
  memcpy(intro->nonce, intro->buf + 32, 32);
  intro->sz = sz;
  // give secret key
  memcpy(intro->secretkey, eph_seckey, 64);
  intro->user = this;
  // set call back hook
  intro->hook = &handle_verify_intro;
  // call
  EnterState(eIntroRecv);
  working = true;
  iwp_call_async_verify_intro(iwp, intro);
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
  if(working)
    return;
  // cancel resend
  llarp_logic_cancel_call(serv->logic, intro_resend_job_id);
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
  decryptedFrames.Process(
      [=](iwp_async_frame &f) { handle_frame_decrypt(&f); });
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
llarp_link_session::keepalive()
{
  send_keepalive(this);
}

void
llarp_link_session::EncryptOutboundFrames()
{
  outboundFrames.Process([&](iwp_async_frame &frame) {
    if(iwp_encrypt_frame(&frame))
      if(llarp_ev_udp_sendto(udp, addr, frame.buf, frame.sz) == -1)
        llarp::LogError("sendto ", addr, " failed");
  });
}

static void
handle_verify_session_start(iwp_async_session_start *s)
{
  llarp_link_session *self = static_cast< llarp_link_session * >(s->user);
  self->working            = false;
  if(!s->buf)
  {
    // verify fail
    llarp::LogWarn("session start verify failed from ", self->addr);
  }
  else
  {
    llarp::LogInfo("session start okay from ", self->addr);
    self->send_LIM();
    self->pump();
  }
}

static void
handle_introack_generated(iwp_async_introack *i)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(i->user);
  link->working            = false;
  if(i->buf)
  {
    link->frame.alive();
    llarp::LogDebug("send introack to ", link->addr, " via ", link->serv->addr);
    if(llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz) != -1)
      link->EnterState(llarp_link_session::State::eIntroAckSent);
  }
  else
  {
    // failed to generate?
    llarp::LogWarn("failed to generate introack");
  }
}

static void
handle_generated_intro(iwp_async_intro *i)
{
  llarp_link_session *link = static_cast< llarp_link_session * >(i->user);
  link->working            = false;
  if(i->buf)
  {
    llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
    delete[] i->buf;
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
  delete i;
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
  auto intro  = new iwp_async_intro;
  intro->buf  = new byte_t[1500];
  size_t w0sz = (llarp_randint() % MAX_PAD);
  intro->sz   = (32 * 3) + w0sz;
  // randomize w0
  if(w0sz)
  {
    crypto->randbytes(intro->buf + (32 * 3), w0sz);
  }

  memcpy(intro->secretkey, eph_seckey, 64);
  // copy in pubkey
  memcpy(intro->remote_pubkey, remote, 32);
  // randomize nonce
  crypto->randbytes(intro->nonce, 32);
  // async generate intro packet
  intro->user = this;
  intro->hook = &handle_generated_intro;
  working     = true;
  iwp_call_async_gen_intro(iwp, intro);
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
    llarp_link_session *self = this;
    decryptedFrames.EmplaceIf([&](iwp_async_frame &f) -> bool {
      if(sz > sizeof(f.buf))
        return false;
      f.sz         = sz;
      f.sessionkey = sessionkey;
      f.iwp        = iwp;
      f.user       = self;
      memcpy(f.buf, buf, sz);
      return iwp_decrypt_frame(&f);
    });
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
  memcpy(start.buf + 64, token, 32);
  if(w2sz)
    crypto->randbytes(start.buf + (32 * 3), w2sz);
  start.remote_pubkey = remote;
  start.secretkey     = eph_seckey;
  start.sessionkey    = sessionkey;
  start.token         = token;
  start.user          = this;
  start.hook          = &handle_generated_session_start;
  working             = true;
  iwp_call_async_gen_session_start(iwp, &start);
}

void
llarp_link_session::on_session_start(const void *buf, size_t sz)
{
  llarp::LogInfo("session start from ", addr);
  if(sz > sizeof(workbuf))
  {
    llarp::LogDebug("session start too big");
    working = false;
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
  crypto->randbytes(token, 32);
  introack.token = token;

  // keys
  introack.remote_pubkey = remote;
  introack.secretkey     = eph_seckey;

  // call
  working       = true;
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
      llarp::LogError("session recv - invalid state: ", state);
      // invalid state?
      break;
  }
}

void
llarp_link_session::encrypt_frame_async_send(const void *buf, size_t sz)
{
  // 64 bytes frame overhead for nonce and hmac
  if(sz + 64 > 1500)
    return;
  llarp_link_session *self = this;
  outboundFrames.EmplaceIf([&](iwp_async_frame &frame) -> bool {
    frame.iwp        = iwp;
    frame.sessionkey = sessionkey;
    frame.user       = self;
    frame.sz         = sz + 64;
    memcpy(frame.buf + 64, buf, sz);
    // maybe add upto 128 random bytes to the packet
    auto padding = llarp_randint() % MAX_PAD;
    if(padding)
      crypto->randbytes(frame.buf + 64 + sz, padding);
    frame.sz += padding;
    return true;
  });
}

void
llarp_link_session::pump()
{
  bool flush = false;
  frame.sendqueue.Process([&](sendbuf_t &msg) {
    encrypt_frame_async_send(msg.data(), msg.size());
    flush = true;
  });
  if(flush)
    PumpCryptoOutbound();
}

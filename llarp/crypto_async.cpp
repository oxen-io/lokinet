#include <llarp/crypto_async.h>
#include <llarp/router_contact.h>
#include <llarp/mem.h>
#include <string.h>
#include <llarp/crypto.hpp>
#include "buffer.hpp"
#include "mem.hpp"

struct llarp_async_iwp
{
  struct llarp_crypto *crypto;
  struct llarp_logic *logic;
  struct llarp_threadpool *worker;
};

namespace iwp
{
  void
  inform_keygen(void *user)
  {
    iwp_async_keygen *keygen = static_cast< iwp_async_keygen * >(user);
    keygen->hook(keygen);
  }

  void
  keygen(void *user)
  {
    iwp_async_keygen *keygen = static_cast< iwp_async_keygen * >(user);
    keygen->iwp->crypto->encryption_keygen(keygen->keybuf);
    llarp_thread_job job;
    job.user = user;
    job.work = &inform_keygen;
    llarp_logic_queue_job(keygen->iwp->logic, job);
  }

  void
  inform_intro(void *user)
  {
    iwp_async_intro *intro = static_cast< iwp_async_intro * >(user);
    intro->hook(intro);
  }

  void
  gen_intro(void *user)
  {
    iwp_async_intro *intro = static_cast< iwp_async_intro * >(user);
    llarp::SharedSecret sharedkey;
    llarp::ShortHash e_k;
    llarp::SymmNonce n;
    llarp_crypto *crypto = intro->iwp->crypto;
    byte_t tmp[64];
    // S = TKE(a.k, b.k, n)
    crypto->transport_dh_client(sharedkey, intro->remote_pubkey,
                                intro->secretkey, intro->nonce);
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    // copy nonce
    memcpy(n, intro->nonce, 24);
    // e_k = HS(b.k + n)
    memcpy(tmp, intro->remote_pubkey, 32);
    memcpy(tmp + 32, intro->nonce, 32);
    crypto->shorthash(e_k, buf);
    // e = SE(a.k, e_k, n[0:24])
    memcpy(intro->buf + 64, llarp::seckey_topublic(intro->secretkey), 32);
    buf.base = intro->buf + 64;
    buf.cur  = buf.base;
    buf.sz   = 32;
    crypto->xchacha20(buf, e_k, n);
    // h = MDS( n + e + w0, S)
    buf.base = intro->buf + 32;
    buf.cur  = buf.base;
    buf.sz   = intro->sz - 32;
    crypto->hmac(intro->buf, buf, sharedkey);
    // inform result
    llarp_logic_queue_job(intro->iwp->logic, {intro, &inform_intro});
  }

  void
  verify_intro(void *user)
  {
    iwp_async_intro *intro = static_cast< iwp_async_intro * >(user);
    auto crypto            = intro->iwp->crypto;
    llarp::SharedSecret sharedkey;
    llarp::ShortHash e_K;
    llarp::SharedSecret h;
    llarp::SymmNonce N;
    byte_t tmp[64];
    auto OurPK = llarp::seckey_topublic(intro->secretkey);
    // e_k = HS(b.k + n)
    memcpy(tmp, OurPK, 32);
    memcpy(tmp + 32, intro->nonce, 32);
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    crypto->shorthash(e_K, buf);

    // a.k = SD(x, e_k, n[0:24])
    memcpy(N, intro->nonce, 24);
    buf.base = intro->remote_pubkey;
    buf.cur  = buf.base;
    buf.sz   = 32;
    memcpy(intro->remote_pubkey, intro->buf + 64, 32);
    crypto->xchacha20(buf, e_K, N);

    // S = TKE(a.k, b.k, n)
    crypto->transport_dh_server(sharedkey, intro->remote_pubkey,
                                intro->secretkey, intro->nonce);
    // h = MDS( n + e + w2 )
    buf.base = intro->buf + 32;
    buf.cur  = buf.base;
    buf.sz   = intro->sz - 32;
    crypto->hmac(h, buf, sharedkey);
    if(memcmp(h, intro->buf, 32))
    {
      // hmac fail
      intro->buf = nullptr;
    }
    // inform result
    llarp_logic_queue_job(intro->iwp->logic, {intro, &inform_intro});
  }

  void
  inform_introack(void *user)
  {
    iwp_async_introack *introack = static_cast< iwp_async_introack * >(user);
    introack->hook(introack);
  }

  void
  verify_introack(void *user)
  {
    iwp_async_introack *introack = static_cast< iwp_async_introack * >(user);
    auto crypto                  = introack->iwp->crypto;
    auto logic                   = introack->iwp->logic;

    llarp::ShortHash digest;
    llarp::SharedSecret sharedkey;

    auto hmac      = introack->buf;
    auto body      = introack->buf + 32;
    auto pubkey    = introack->remote_pubkey;
    auto secretkey = introack->secretkey;
    auto nonce     = introack->buf + 32;
    auto token     = introack->buf + 64;
    size_t bodysz  = introack->sz - 32;
    llarp_buffer_t buf;
    buf.base = body;
    buf.cur  = body;
    buf.sz   = bodysz;

    // S = TKE(a.k, b.k, n)
    crypto->transport_dh_client(sharedkey, pubkey, secretkey, nonce);

    // h = MDS(n + x + w1, S)
    crypto->hmac(digest, buf, sharedkey);

    if(!llarp_eq(digest, hmac, 32))
    {
      // fail to verify hmac
      introack->buf = nullptr;
    }
    else
    {
      buf.base = token;
      buf.sz   = 32;
      // token = SD(S, x, n[0:24])
      crypto->xchacha20(buf, sharedkey, nonce);
      // copy token
      memcpy(introack->token, token, 32);
    }
    llarp_logic_queue_job(logic, {introack, &inform_introack});
  }

  void
  gen_introack(void *user)
  {
    iwp_async_introack *introack = static_cast< iwp_async_introack * >(user);
    llarp::SharedSecret sharedkey;
    auto crypto    = introack->iwp->crypto;
    auto pubkey    = introack->remote_pubkey;
    auto secretkey = introack->secretkey;
    auto nonce     = introack->nonce;
    // S = TKE(a.k, b.k, n)
    crypto->transport_dh_server(sharedkey, pubkey, secretkey, nonce);

    // x = SE(S, token, n[0:24])
    llarp_buffer_t buf;
    buf.base = introack->buf + 64;
    buf.sz   = 32;
    buf.cur  = buf.base;
    memcpy(buf.base, introack->token, 32);
    crypto->xchacha20(buf, sharedkey, nonce);

    // h = MDS(n + x + w1, S)
    buf.base = introack->buf + 32;
    buf.sz   = introack->sz - 32;
    buf.cur  = buf.base;
    crypto->hmac(introack->buf, buf, sharedkey);

    llarp_logic_queue_job(introack->iwp->logic, {introack, &inform_introack});
  }

  void
  inform_session_start(void *user)
  {
    iwp_async_session_start *session =
        static_cast< iwp_async_session_start * >(user);
    session->hook(session);
  }

  void
  gen_session_start(void *user)
  {
    iwp_async_session_start *session =
        static_cast< iwp_async_session_start * >(user);
    auto crypto = session->iwp->crypto;

    auto dh        = crypto->transport_dh_client;
    auto shorthash = crypto->shorthash;
    auto hmac      = crypto->hmac;
    auto encrypt   = crypto->xchacha20;

    auto logic = session->iwp->logic;
    auto a_sK  = session->secretkey;
    auto b_K   = session->remote_pubkey;
    auto N     = session->nonce;
    auto token = session->token;
    auto K     = session->sessionkey;

    llarp::SharedSecret e_K;
    llarp::ShortHash T;

    byte_t tmp[64];
    llarp_buffer_t buf;
    llarp::StackBuffer< decltype(tmp) >(buf, tmp);

    // T = HS(token + n)
    memcpy(tmp, token, 32);
    memcpy(tmp + 32, N, 32);
    shorthash(T, buf);

    // e_K = TKE(a.k, b.k, n)
    dh(e_K, b_K, a_sK, N);
    // K = TKE(a.k, b.k, T)
    dh(K, b_K, a_sK, T);

    // x = SE(e_K, token, n[0:24])
    buf.base = (session->buf + 64);
    buf.sz   = 32;
    memcpy(buf.base, token, 32);
    encrypt(buf, e_K, N);

    // h = MDS(n + x + w2, e_K)
    buf.base = (session->buf + 32);
    buf.sz   = session->sz - 32;
    hmac(session->buf, buf, e_K);

    llarp_logic_queue_job(logic, {user, &inform_session_start});
  }

  void
  verify_session_start(void *user)
  {
    iwp_async_session_start *session =
        static_cast< iwp_async_session_start * >(user);
    auto crypto = session->iwp->crypto;

    auto dh        = crypto->transport_dh_server;
    auto shorthash = crypto->shorthash;
    auto hmac      = crypto->hmac;
    auto decrypt   = crypto->xchacha20;

    auto logic = session->iwp->logic;
    auto b_sK  = session->secretkey;
    auto a_K   = session->remote_pubkey;
    auto N     = session->nonce;
    auto token = session->token;
    auto K     = session->sessionkey;

    llarp::SharedSecret e_K;
    llarp::ShortHash T;

    byte_t tmp[64];

    llarp_buffer_t buf;

    // e_K = TKE(a.k, b.k, N)
    dh(e_K, a_K, b_sK, N);
    // h = MDS( n + x + w2, e_K)
    buf.base = session->buf + 32;
    buf.cur  = buf.base;
    buf.sz   = session->sz - 32;
    hmac(tmp, buf, e_K);
    if(memcmp(tmp, session->buf, 32) == 0)
    {
      // hmac good
      buf.base = session->buf + 64;
      buf.cur  = buf.base;
      buf.sz   = 32;
      // token = SD(e_K, x, n[0:24])
      decrypt(buf, e_K, N);
      // ensure it's the same token
      if(memcmp(buf.base, token, 32) == 0)
      {
        // T = HS(token + n)
        memcpy(tmp, token, 32);
        memcpy(tmp + 32, N, 32);
        buf.base = tmp;
        buf.cur  = buf.base;
        buf.sz   = sizeof(tmp);
        shorthash(T, buf);
        // K = TKE(a.k, b.k, T)
        dh(K, a_K, b_sK, T);
      }
      else  // token missmatch
      {
        session->buf = nullptr;
      }
    }
    else  // hmac fail
      session->buf = nullptr;

    llarp_logic_queue_job(logic, {user, &inform_session_start});
  }

  void
  inform_frame_done(void *user)
  {
    iwp_async_frame *frame = static_cast< iwp_async_frame * >(user);
    frame->hook(frame);
    delete frame;
  }

  void
  hmac_then_decrypt(void *user)
  {
    iwp_async_frame *frame = static_cast< iwp_async_frame * >(user);
    auto crypto            = frame->iwp->crypto;
    byte_t *hmac           = frame->buf;
    byte_t *nonce          = frame->buf + 32;
    byte_t *body           = frame->buf + 64;

    llarp::ShortHash digest;

    llarp_buffer_t buf;
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = frame->sz - 32;

    // h = MDS(n + x, S)
    crypto->hmac(digest, buf, frame->sessionkey);
    // check hmac
    frame->success = memcmp(digest, hmac, 32) == 0;
    // x = SE(S, p, n[0:24])
    buf.base = body;
    buf.cur  = buf.base;
    buf.sz   = frame->sz - 64;
    crypto->xchacha20(buf, frame->sessionkey, nonce);
    // inform result
    llarp_logic_queue_job(frame->iwp->logic, {frame, &inform_frame_done});
  }

  void
  encrypt_then_hmac(void *user)
  {
    iwp_async_frame *frame = static_cast< iwp_async_frame * >(user);
    auto crypto            = frame->iwp->crypto;
    byte_t *hmac           = frame->buf;
    byte_t *nonce          = frame->buf + 32;
    byte_t *body           = frame->buf + 64;

    llarp_buffer_t buf;
    buf.base = body;
    buf.cur  = buf.base;
    buf.sz   = frame->sz - 64;

    // randomize N
    crypto->randbytes(nonce, 32);
    // x = SE(S, p, n[0:24])
    crypto->xchacha20(buf, frame->sessionkey, nonce);
    // h = MDS(n + x, S)
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = frame->sz - 32;
    crypto->hmac(hmac, buf, frame->sessionkey);
    // call result RIGHT HERE
    frame->hook(frame);
    delete frame;
  }
}

extern "C" {

void
iwp_call_async_keygen(struct llarp_async_iwp *iwp,
                      struct iwp_async_keygen *keygen)
{
  keygen->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {keygen, &iwp::keygen});
}

void
iwp_call_async_gen_intro(struct llarp_async_iwp *iwp,
                         struct iwp_async_intro *intro)
{
  intro->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {intro, &iwp::gen_intro});
}

void
iwp_call_async_verify_introack(struct llarp_async_iwp *iwp,
                               struct iwp_async_introack *introack)
{
  introack->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {introack, &iwp::verify_introack});
}

void
iwp_call_async_gen_session_start(struct llarp_async_iwp *iwp,
                                 struct iwp_async_session_start *session)
{
  session->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {session, &iwp::gen_session_start});
}

void
iwp_call_async_verify_intro(struct llarp_async_iwp *iwp,
                            struct iwp_async_intro *intro)
{
  intro->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {intro, &iwp::verify_intro});
}

void
iwp_call_async_gen_introack(struct llarp_async_iwp *iwp,
                            struct iwp_async_introack *introack)
{
  introack->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {introack, &iwp::gen_introack});
}

void
iwp_call_async_frame_decrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame)
{
  frame->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {frame, &iwp::hmac_then_decrypt});
}

void
iwp_call_async_frame_encrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame)
{
  frame->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker, {frame, &iwp::encrypt_then_hmac});
}

void
iwp_call_async_verify_session_start(struct llarp_async_iwp *iwp,
                                    struct iwp_async_session_start *session)
{
  session->iwp = iwp;
  llarp_threadpool_queue_job(iwp->worker,
                             {session, &iwp::verify_session_start});
}

struct llarp_async_iwp *
llarp_async_iwp_new(struct llarp_crypto *crypto, struct llarp_logic *logic,
                    struct llarp_threadpool *worker)
{
  llarp_async_iwp *iwp = new llarp_async_iwp;
  if(iwp)
  {
    iwp->crypto = crypto;
    iwp->logic  = logic;
    iwp->worker = worker;
  }
  return iwp;
}

void
llarp_async_iwp_free(struct llarp_async_iwp *iwp)
{
  delete iwp;
}

}

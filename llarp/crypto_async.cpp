#include <llarp/crypto_async.h>
#include <llarp/mem.h>
#include <string.h>
#include "buffer.hpp"
#include "mem.hpp"

struct llarp_async_iwp
{
  struct llarp_alloc *mem;
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
    keygen->iwp->crypto->keygen(keygen->keybuf);
    llarp_thread_job job = {.user = user, .work = &inform_keygen};
    llarp_logic_queue_job(keygen->iwp->logic, job);
  }

  void
  inform_gen_intro(void *user)
  {
    iwp_async_intro *intro = static_cast< iwp_async_intro * >(user);
    intro->hook(intro);
  }

  void
  gen_intro(void *user)
  {
    iwp_async_intro *intro = static_cast< iwp_async_intro * >(user);
    llarp_sharedkey_t sharedkey;
    llarp_shorthash_t e_k;
    llarp_nonce_t n;
    llarp_crypto *crypto = intro->iwp->crypto;
    byte_t tmp[64];
    // S = TKE(a.k, b.k, n)
    crypto->transport_dh_client(sharedkey, intro->remote_pubkey,
                                intro->secretkey, intro->nonce);
    llarp_buffer_t buf;
    llarp::StackBuffer< decltype(tmp) >(buf, tmp);
    // copy nonce
    memcpy(n, intro->nonce, 24);
    // e_k = HS(b.k + n)
    memcpy(tmp, intro->remote_pubkey, 32);
    memcpy(tmp + 32, intro->nonce, 32);
    crypto->shorthash(e_k, buf);
    // e = SE(a.k, e_k, n[0:24])
    memcpy(intro->buf + 32, llarp_seckey_topublic(intro->secretkey), 32);
    buf.base = intro->buf + 32;
    buf.sz   = 32;
    crypto->xchacha20(buf, e_k, n);
    // h = MDS( n + e + w0, S)
    buf.sz = intro->sz - 32;
    crypto->hmac(intro->buf, buf, sharedkey);
    // inform result
    llarp_logic_queue_job(intro->iwp->logic, {intro, &inform_gen_intro});
  }

  void
  inform_verify_introack(void *user)
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

    llarp_thread_job job = {.user = user, .work = &inform_verify_introack};

    llarp_hmac_t digest;
    llarp_sharedkey_t sharedkey;

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
      llarp_logic_queue_job(logic, job);
      return;
    }
    buf.base = token;
    buf.sz   = 32;
    // token = SD(S, x, n[0:24])
    crypto->xchacha20(buf, sharedkey, nonce);
    // copy token
    memcpy(introack->token, token, 32);
    llarp_logic_queue_job(logic, job);
  }

  void
  inform_gen_session_start(void *user)
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

    llarp_sharedkey_t e_K;
    llarp_shorthash_t T;

    byte_t tmp[64];
    llarp_buffer_t buf;
    llarp::StackBuffer< decltype(tmp) >(buf, tmp);

    // T = HS(token + n)
    memcpy(tmp, token, 32);
    memcpy(tmp + 32, N, 32);
    shorthash(T, buf);

    // e_K = TKE(a.k, b.k, N)
    dh(e_K, b_K, a_sK, N);
    // K = TKE(a.k, b.k, T)
    dh(K, b_K, a_sK, T);

    // x = SE(e_K, token, n[0:24])
    buf.base = (session->buf + 32);
    buf.sz   = 32;
    memcpy(buf.base, token, 32);
    encrypt(buf, e_K, N);

    // h = MDS(n + x + w2, e_K)
    buf.base = (session->buf + 32);
    buf.sz   = session->sz - 32;
    hmac(session->buf, buf, e_K);

    // K = TKE(a.k, b.k, T)
    dh(K, b_K, a_sK, T);

    llarp_logic_queue_job(logic, {user, &inform_gen_session_start});
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
iwp_call_async_frame_decrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame)
{
}

struct llarp_async_iwp *
llarp_async_iwp_new(struct llarp_alloc *mem, struct llarp_crypto *crypto,
                    struct llarp_logic *logic, struct llarp_threadpool *worker)
{
  llarp_async_iwp *iwp = new llarp_async_iwp;
  if(iwp)
  {
    iwp->mem    = mem;
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

#include <llarp/crypto_async.h>
#include <llarp/mem.h>
#include <string.h>
#include "mem.hpp"

struct llarp_async_iwp
{
  struct llarp_alloc * mem;
  struct llarp_crypto * crypto;
  struct llarp_logic * logic;
  struct llarp_threadpool * worker;
};

void iwp_inform_keygen(void * user)
{
  iwp_async_keygen * keygen = static_cast<iwp_async_keygen *>(user);
  keygen->hook(keygen);
}

void iwp_do_keygen(void * user)
{
  iwp_async_keygen * keygen = static_cast<iwp_async_keygen *>(user);
  keygen->iwp->crypto->keygen(keygen->keybuf);
  llarp_thread_job job = {
    .user = user,
    .work = iwp_inform_keygen
  };
  llarp_logic_queue_job(keygen->iwp->logic, job);
}

void iwp_inform_genintro(void * user)
{
  iwp_async_intro * intro = static_cast<iwp_async_intro *>(user);
  intro->hook(intro);
}

void iwp_do_genintro(void * user)
{
  iwp_async_intro * intro = static_cast<iwp_async_intro *>(user);
  llarp_sharedkey_t sharedkey;
  llarp_shorthash_t e_k;
  llarp_buffer_t buf;
  llarp_crypto * crypto = intro->iwp->crypto;
  uint8_t tmp[64];
  llarp_thread_job job = {
    .user = user,
    .work = &iwp_inform_genintro
  };
  // S = TKE(a.k, b.k, n)
  crypto->transport_dh_client(sharedkey, intro->remote_pubkey, intro->secretkey, intro->nonce);
  
  buf.base = (char*)tmp;
  buf.sz = sizeof(tmp);
  // e_k = HS(b.k + n)
  memcpy(tmp, intro->remote_pubkey, 32);
  memcpy(tmp + 32, intro->nonce, 32);
  crypto->shorthash(&e_k, buf);
  // e = SE(a.k, e_k, n[0:24])
  memcpy(intro->buf + 32, llarp_seckey_topublic(intro->secretkey), 32);
  buf.base = (char*) intro->buf + 32;
  buf.sz = 32;
  crypto->xchacha20(buf, e_k, intro->nonce);
  // h = MDS( n + e + w0, S)
  buf.sz = intro->sz - 32;
  crypto->hmac(intro->buf, buf, sharedkey);
  // inform result
  llarp_logic_queue_job(intro->iwp->logic, job);
}


void iwp_inform_verify_introack(void * user)
{
  iwp_async_introack * introack =  static_cast<iwp_async_introack *>(user);
  introack->hook(introack);
}


void iwp_do_verify_introack(void * user)
{
  iwp_async_introack * introack = static_cast<iwp_async_introack *>(user);
  llarp_crypto * crypto = introack->iwp->crypto;
  llarp_logic * logic = introack->iwp->logic;

  llarp_thread_job job = {
    .user = user,
    .work = &iwp_inform_verify_introack
  };

  llarp_hmac_t digest;
  llarp_sharedkey_t sharedkey;
  
  uint8_t * hmac = introack->buf;
  uint8_t * body = introack->buf + 32;
  uint8_t * pubkey = introack->remote_pubkey;
  uint8_t * secretkey = introack->secretkey;
  uint8_t * nonce = introack->buf + 32;
  uint8_t * token = introack->buf + 64;
  size_t bodysz = introack->sz - 32;
  llarp_buffer_t buf;
  buf.base = (char*) body;
  buf.sz = bodysz;

  // S = TKE(a.k, b.k, n)
  crypto->transport_dh_client(sharedkey, pubkey, secretkey, nonce);
  
  // h = MDS(n + x + w1, S)
  crypto->hmac(digest, buf, sharedkey);
  
  if(!llarp_eq(digest, hmac, 32))
  {
    // fail to verify hmac
    introack->buf = 0;
    llarp_logic_queue_job(logic, job);
    return;
  }
  buf.base = (char *) token;
  buf.sz = 32;
  // token = SD(S, x, n[0:24])
  crypto->xchacha20(buf, sharedkey, nonce);
  // copy token
  memcpy(introack->token, token, 32);
}

extern "C" {

void iwp_call_async_keygen(struct llarp_async_iwp * iwp, struct iwp_async_keygen * keygen)
{
  keygen->iwp = iwp;
  struct llarp_thread_job job = {
    .user = keygen,
    .work = &iwp_do_keygen
  };
  llarp_threadpool_queue_job(iwp->worker, job);
}

void iwp_call_async_gen_intro(struct llarp_async_iwp * iwp, struct iwp_async_intro * intro)
{
  
  intro->iwp = iwp;
  struct llarp_thread_job job = {
    .user = intro,
    .work = &iwp_do_genintro
  };
  llarp_threadpool_queue_job(iwp->worker, job);
}

void iwp_call_async_verify_introack(struct llarp_async_iwp * iwp, struct iwp_async_introack * introack)
{
  introack->iwp = iwp;
  struct llarp_thread_job job = {
    .user = introack,
    .work = &iwp_do_verify_introack
  };
  llarp_threadpool_queue_job(iwp->worker, job);
}

void iwp_call_async_gen_session_start(struct llarp_async_iwp * iwp, struct iwp_async_session_start * session)
{
  session->iwp = iwp;
}

void iwp_call_async_frame_decrypt(struct llarp_async_iwp * iwp, struct iwp_async_frame * frame)
{
}


struct llarp_async_iwp * llarp_async_iwp_new(struct llarp_alloc * mem,
                                             struct llarp_crypto * crypto,
                                             struct llarp_logic * logic,
                                             struct llarp_threadpool * worker)
{
  struct llarp_async_iwp * iwp = llarp::Alloc<llarp_async_iwp>(mem);
  if(iwp)
  {
    iwp->mem = mem;
    iwp->crypto = crypto;
    iwp->logic = logic;
    iwp->worker = worker;
  }
  return iwp;
}

}

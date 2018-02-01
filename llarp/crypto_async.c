#include <llarp/crypto_async.h>
#include <llarp/mem.h>
#include <string.h>

struct llarp_async_dh {
  llarp_dh_func client;
  llarp_dh_func server;
  struct llarp_threadpool *tp;
  struct llarp_ev_caller *caller;
  llarp_seckey_t ourkey;
};

struct llarp_dh_internal {
  llarp_pubkey_t theirkey;
  uint8_t * ourkey;
  llarp_tunnel_nounce_t nounce;
  struct llarp_dh_result result;
  llarp_dh_func func;
};

static void llarp_crypto_dh_work(void *user) {
  struct llarp_dh_internal *impl = (struct llarp_dh_internal *)user;
  impl->func(&impl->result.sharedkey, impl->theirkey, impl->nounce, impl->ourkey);
}

static void llarp_crypto_dh_result(struct llarp_ev_async_call *call) {
  struct llarp_dh_internal *impl = (struct llarp_dh_internal *)call->user;
  impl->result.hook(&impl->result);
  llarp_g_mem.free(impl);
}

static void llarp_async_dh_exec(struct llarp_async_dh *dh, llarp_dh_func func,
                                llarp_pubkey_t theirkey,
                                llarp_tunnel_nounce_t nounce,
                                llarp_dh_complete_hook result, void *user) {
  struct llarp_dh_internal *impl =
      llarp_g_mem.alloc(sizeof(struct llarp_dh_internal), 32);
  memcpy(impl->theirkey, theirkey, sizeof(llarp_pubkey_t));
  memcpy(impl->nounce, nounce, sizeof(llarp_tunnel_nounce_t));
  impl->ourkey = dh->ourkey;
  impl->result.user = user;
  impl->result.hook = result;
  impl->func = func;
  struct llarp_thread_job job = {.caller = dh->caller,
                                 .data = impl,
                                 .user = impl,
                                 .work = &llarp_crypto_dh_work};
  llarp_threadpool_queue_job(dh->tp, job);
}

void llarp_async_client_dh(struct llarp_async_dh *dh,
                           llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user) {
  llarp_async_dh_exec(dh, dh->client, theirkey, nounce, result, user);
}

void llarp_async_server_dh(struct llarp_async_dh *dh,
                           llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user) {
  llarp_async_dh_exec(dh, dh->server, theirkey, nounce, result, user);
}

struct llarp_async_dh *llarp_async_dh_new(
  llarp_seckey_t ourkey, 
  struct llarp_crypto *crypto,
  struct llarp_ev_loop *ev,
  struct llarp_threadpool *tp) {
  struct llarp_async_dh *dh =
      llarp_g_mem.alloc(sizeof(struct llarp_async_dh), 16);
  dh->client = crypto->dh_client;
  dh->server = crypto->dh_server;
  memcpy(dh->ourkey, ourkey, sizeof(llarp_seckey_t));
  dh->tp = tp;
  dh->caller = llarp_ev_prepare_async(ev, &llarp_crypto_dh_result);
  return dh;
}

void llarp_async_dh_free(struct llarp_async_dh **dh) {
  if (*dh) {
    llarp_ev_caller_stop((*dh)->caller);
    llarp_g_mem.free(*dh);
    *dh = NULL;
  }
}


struct llarp_async_cipher_internal
{
  uint8_t * key;
  llarp_nounce_t nounce;
  struct llarp_cipher_result result;
  llarp_sym_cipher_func func;
};

struct llarp_async_cipher
{
  llarp_sharedkey_t key;
  struct llarp_threadpool *tp;
  struct llarp_ev_caller *caller;
  llarp_sym_cipher_func func;
};

static void llarp_crypto_cipher_result(struct llarp_ev_async_call * job)
{
  struct llarp_async_cipher_internal * impl = (struct llarp_async_cipher_internal *) job->user;
  impl->result.hook(&impl->result);
  llarp_g_mem.free(impl);
}

static void llarp_crypto_cipher_work(void * work)
{
  struct llarp_async_cipher_internal * impl = (struct llarp_async_cipher_internal *) work;
  impl->func(impl->result.buff, impl->key, impl->nounce);
}

void llarp_async_cipher_queue_op(struct llarp_async_cipher * c, llarp_buffer_t * buff, llarp_nounce_t n, llarp_cipher_complete_hook h, void * user)
{
  struct llarp_async_cipher_internal * impl = llarp_g_mem.alloc(sizeof(struct llarp_async_cipher_internal), 16);
  impl->key = c->key;
  memcpy(impl->nounce, n, sizeof(llarp_nounce_t));
  impl->result.user = user;
  impl->result.buff.base = buff->base;
  impl->result.buff.sz = buff->sz;
  impl->result.hook = h;
  impl->func = c->func;
  struct llarp_thread_job job = {.caller = c->caller,
                                 .data = impl,
                                 .user = impl,
                                 .work = &llarp_crypto_cipher_work};
  llarp_threadpool_queue_job(c->tp, job);
}

struct llarp_async_cipher * llarp_async_cipher_new(llarp_sharedkey_t key,
                                                   struct llarp_crypto * crypto,
                                                   struct llarp_ev_loop * ev,
                                                   struct llarp_threadpool *tp)
{
  struct llarp_async_cipher * cipher = llarp_g_mem.alloc(sizeof(struct llarp_async_cipher), 16);
  cipher->func = crypto->xchacha20;
  cipher->tp = tp;
  cipher->caller = llarp_ev_prepare_async(ev, &llarp_crypto_cipher_result);
  memcpy(cipher->key, key, sizeof(llarp_sharedkey_t));
  return cipher;
}

void llarp_async_cipher_free(struct llarp_async_cipher **c) {
  if (*c) {
    llarp_ev_caller_stop((*c)->caller);
    llarp_g_mem.free(*c);
    *c = NULL;
  }
}

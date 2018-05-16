#include <llarp/crypto_async.h>
#include <llarp/mem.h>
#include <string.h>

struct llarp_async_dh {
  llarp_dh_func client;
  llarp_dh_func server;
  struct llarp_threadpool *worker;
  struct llarp_threadpool *result;
  llarp_seckey_t ourkey;
  struct llarp_alloc * mem;
};

struct llarp_dh_internal {
  llarp_pubkey_t theirkey;
  llarp_tunnel_nounce_t nounce;
  struct llarp_dh_result result;
  llarp_dh_func func;
  struct llarp_async_dh *parent;
};

static void llarp_crypto_dh_result(void *call) {
  struct llarp_dh_internal *impl = (struct llarp_dh_internal *)call;
  impl->result.hook(&impl->result);
  impl->parent->mem->free(impl->parent->mem, impl);
}

static void llarp_crypto_dh_work(void *user) {
  struct llarp_dh_internal *impl = (struct llarp_dh_internal *)user;
  impl->func(&impl->result.sharedkey, impl->theirkey, impl->nounce,
             impl->parent->ourkey);
  struct llarp_thread_job job = {.user = impl, .work = &llarp_crypto_dh_result};
  llarp_threadpool_queue_job(impl->parent->result, job);
}

static void llarp_async_dh_exec(struct llarp_async_dh *dh, llarp_dh_func func,
                                llarp_pubkey_t theirkey,
                                llarp_tunnel_nounce_t nounce,
                                llarp_dh_complete_hook result, void *user) {
  struct llarp_dh_internal *impl =
    dh->mem->alloc(dh->mem, sizeof(struct llarp_dh_internal), 32);
  struct llarp_thread_job job = {.user = impl, .work = &llarp_crypto_dh_work};
  memcpy(impl->theirkey, theirkey, sizeof(llarp_pubkey_t));
  memcpy(impl->nounce, nounce, sizeof(llarp_tunnel_nounce_t));
  impl->parent = dh;
  impl->result.user = user;
  impl->result.hook = result;
  impl->func = func;
  llarp_threadpool_queue_job(dh->worker, job);
}

void llarp_async_client_dh(struct llarp_async_dh *dh, llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user) {
  llarp_async_dh_exec(dh, dh->client, theirkey, nounce, result, user);
}

void llarp_async_server_dh(struct llarp_async_dh *dh, llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user) {
  llarp_async_dh_exec(dh, dh->server, theirkey, nounce, result, user);
}

struct llarp_async_dh *llarp_async_dh_new(
  struct llarp_alloc * mem,
  llarp_seckey_t ourkey,
  struct llarp_crypto *crypto,
  struct llarp_threadpool *result,
  struct llarp_threadpool *worker) {
  struct llarp_async_dh *dh =
    mem->alloc(mem, sizeof(struct llarp_async_dh), 16);
  dh->mem = mem;
  dh->client = crypto->dh_client;
  dh->server = crypto->dh_server;
  memcpy(dh->ourkey, ourkey, sizeof(llarp_seckey_t));
  dh->result = result;
  dh->worker = worker;
  return dh;
}

void llarp_async_dh_free(struct llarp_async_dh **dh) {
  if (*dh) {
    struct llarp_alloc * mem = (*dh)->mem;
    mem->free(mem, *dh);
    *dh = NULL;
  }
}

struct llarp_async_cipher_internal {
  llarp_nounce_t nounce;
  struct llarp_cipher_result result;
  struct llarp_async_cipher *parent;
  llarp_sym_cipher_func func;
};

struct llarp_async_cipher {
  llarp_sharedkey_t key;
  struct llarp_threadpool *worker;
  struct llarp_threadpool *result;
  llarp_sym_cipher_func func;
  struct llarp_alloc * mem;
};

static void llarp_crypto_cipher_result(void *user) {
  struct llarp_async_cipher_internal *impl =
      (struct llarp_async_cipher_internal *)user;
  struct llarp_alloc * mem = impl->parent->mem;
  impl->result.hook(&impl->result);
  mem->free(mem, impl);
}

static void llarp_crypto_cipher_work(void *work) {
  struct llarp_async_cipher_internal *impl =
      (struct llarp_async_cipher_internal *)work;
  impl->func(impl->result.buff, impl->parent->key, impl->nounce);
  struct llarp_thread_job job = {.user = impl,
                                 .work = &llarp_crypto_cipher_result};
  llarp_threadpool_queue_job(impl->parent->result, job);
}

void llarp_async_cipher_queue_op(struct llarp_async_cipher *c,
                                 llarp_buffer_t *buff, llarp_nounce_t n,
                                 llarp_cipher_complete_hook h, void *user) {
  struct llarp_alloc * mem = c->mem;
  struct llarp_async_cipher_internal *impl =
    mem->alloc(mem, sizeof(struct llarp_async_cipher_internal), 16);
  impl->parent = c;
  memcpy(impl->nounce, n, sizeof(llarp_nounce_t));
  impl->result.user = user;
  impl->result.buff.base = buff->base;
  impl->result.buff.sz = buff->sz;
  impl->result.hook = h;
  impl->func = c->func;
  struct llarp_thread_job job = {.user = impl,
                                 .work = &llarp_crypto_cipher_work};
  llarp_threadpool_queue_job(c->worker, job);
}

struct llarp_async_cipher *llarp_async_cipher_new(
  struct llarp_alloc * mem,
  llarp_sharedkey_t key, struct llarp_crypto *crypto,
  struct llarp_threadpool *result, struct llarp_threadpool *worker) {
  
  struct llarp_async_cipher *cipher =
    mem->alloc(mem, sizeof(struct llarp_async_cipher), 16);
  cipher->mem = mem;
  cipher->func = crypto->xchacha20;
  cipher->result = result;
  cipher->worker = worker;
  memcpy(cipher->key, key, sizeof(llarp_sharedkey_t));
  return cipher;
}

void llarp_async_cipher_free(struct llarp_async_cipher **c) {
  if (*c) {
    struct llarp_alloc * mem = (*c)->mem;
    mem->free(mem, *c);
    *c = NULL;
  }
}

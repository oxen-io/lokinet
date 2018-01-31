#include <llarp/crypto_async.h>
#include <llarp/mem.h>
#include <string.h>

struct llarp_async_dh
{
  llarp_dh_func client;
  llarp_dh_func server;
  struct llarp_threadpool * tp;
  struct llarp_ev_caller * caller;
};

struct llarp_dh_internal
{
  llarp_dh_func func;
  llarp_pubkey_t theirkey;
  llarp_seckey_t ourkey;
  llarp_tunnel_nounce_t nounce;
  struct llarp_dh_result result;
};

static void llarp_crypto_dh_work(void * user)
{
  struct llarp_dh_internal * impl = (struct llarp_dh_internal *)user;
  impl->func(&impl->result.result, impl->theirkey, impl->nounce, impl->ourkey);
}

static void llarp_crypto_dh_result(struct llarp_ev_async_call * call)
{
  struct llarp_dh_internal * impl = (struct llarp_dh_internal *) call->user;
  impl->result.hook(&impl->result);
  llarp_g_mem.free(impl);
}

static void llarp_async_dh_exec(struct llarp_async_dh * dh, llarp_dh_func func, llarp_seckey_t ourkey, llarp_pubkey_t theirkey, llarp_tunnel_nounce_t nounce, llarp_dh_complete_hook result, void * user)
{
  struct llarp_dh_internal * impl = llarp_g_mem.alloc(sizeof(struct llarp_dh_internal), 16);
  memcpy(impl->theirkey, theirkey, sizeof(llarp_pubkey_t));
  memcpy(impl->ourkey, ourkey, sizeof(llarp_seckey_t));
  memcpy(impl->nounce, nounce, sizeof(llarp_tunnel_nounce_t));
  impl->result.impl = impl;
  impl->result.user = user;
  impl->result.hook = result;
  impl->func = func;
  struct llarp_thread_job job = {
    .caller = dh->caller,
    .data = impl,
    .user = impl,
    .work = &llarp_crypto_dh_work
  };
  llarp_threadpool_queue_job(dh->tp, job);
}


void llarp_async_client_dh(struct llarp_async_dh * dh, llarp_seckey_t ourkey, llarp_pubkey_t theirkey, llarp_tunnel_nounce_t nounce, llarp_dh_complete_hook result, void * user)
{
  llarp_async_dh_exec(dh, dh->client, ourkey, theirkey, nounce, result, user);
}

void llarp_async_server_dh(struct llarp_async_dh * dh, llarp_seckey_t ourkey, llarp_pubkey_t theirkey, llarp_tunnel_nounce_t nounce, llarp_dh_complete_hook result, void * user)
{
  llarp_async_dh_exec(dh, dh->server, ourkey, theirkey, nounce, result, user);
}



struct llarp_async_dh * llarp_async_dh_new(struct llarp_crypto * crypto, struct llarp_ev_loop * ev, struct llarp_threadpool * tp)
{
  struct llarp_async_dh * dh = llarp_g_mem.alloc(sizeof(struct llarp_async_dh), 16);
  dh->client = crypto->dh_client;
  dh->server = crypto->dh_server;
  dh->tp = tp;
  dh->caller = llarp_ev_prepare_async(ev, &llarp_crypto_dh_result);
  return dh;
}

void llarp_async_dh_free(struct llarp_async_dh ** dh)
{
  if(*dh)
  {
    llarp_ev_caller_stop((*dh)->caller);
    llarp_g_mem.free(*dh);
    *dh = NULL;
  }
}

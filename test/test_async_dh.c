#include <llarp/crypto_async.h>
#include <llarp/mem.h>

#include <stdio.h>

struct dh_bench_main {
  size_t completed;
  size_t num;
  struct llarp_threadpool *result;
  struct llarp_async_dh *dh;
};

static void handle_dh_complete(struct llarp_dh_result *res) {
  struct dh_bench_main *m = (struct dh_bench_main *)res->user;

  m->completed++;
  if (m->completed % 10000 == 0) printf("completed %ld\n", m->completed);
  if (m->completed == m->num) {
    printf("done\n");
    llarp_threadpool_stop(m->result);
  }
}

int main(int argc, char *argv[]) {
  struct dh_bench_main dh_main;
  struct llarp_crypto crypto;
  struct llarp_threadpool *tp;

  llarp_mem_stdlib();
  llarp_crypto_libsodium_init(&crypto);

  tp = llarp_init_threadpool(8);
  dh_main.result = llarp_init_threadpool(1);
  llarp_threadpool_start(dh_main.result);
  
  dh_main.num = 500000;
  dh_main.completed = 0;
  llarp_seckey_t ourkey;
  llarp_seckey_t theirkey;

  crypto.keygen(&ourkey);
  crypto.keygen(&theirkey);

  dh_main.dh = llarp_async_dh_new(ourkey, &crypto, dh_main.result, tp);
  llarp_threadpool_start(tp);

  llarp_tunnel_nounce_t nounce;
  llarp_buffer_t n_buff;
  n_buff.base = nounce;
  n_buff.cur = n_buff.base;
  n_buff.sz = sizeof(llarp_tunnel_nounce_t);

  uint8_t *theirpubkey = llarp_seckey_topublic(theirkey);

  size_t sz = dh_main.num;
  printf("starting %ld dh jobs\n", sz);
  /* do work here */
  while (sz--) {
    crypto.randomize(n_buff);
    llarp_async_client_dh(dh_main.dh, theirpubkey, nounce, handle_dh_complete,
                          &dh_main);
  }
  printf("started %ld dh jobs\n", dh_main.num);
  llarp_threadpool_wait(dh_main.result);
  llarp_threadpool_join(dh_main.result);
  llarp_threadpool_stop(tp);
  llarp_threadpool_join(tp);
  
  llarp_free_threadpool(&tp);
  llarp_free_threadpool(&dh_main.result);
  
  llarp_async_dh_free(&dh_main.dh);
  printf("did %ld of %ld work\n", dh_main.completed, dh_main.num);
  return 0;
}

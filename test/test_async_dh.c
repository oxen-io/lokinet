#include <llarp/crypto_async.h>
#include <llarp/mem.h>

#include <stdio.h>

struct dh_bench_main {
  size_t completed;
  size_t num;
  struct llarp_ev_loop *ev;
  struct llarp_async_dh *dh;
};

static void handle_dh_complete(struct llarp_dh_result *res) {
  struct dh_bench_main *m = (struct dh_bench_main *)res->user;
  m->completed++;
  if (m->completed % 1000 == 0) printf("completed %ld\n", m->completed);
  if (m->completed == m->num) {
    printf("we done\n");
    llarp_ev_loop_stop(m->ev);
  }
}

int main(int argc, char *argv[]) {
  struct dh_bench_main dh_main;
  struct llarp_crypto crypto;
  struct llarp_threadpool *tp;

  llarp_mem_jemalloc();
  llarp_crypto_libsodium_init(&crypto);
  llarp_ev_loop_alloc(&dh_main.ev);

  tp = llarp_init_threadpool(8);
  dh_main.dh = llarp_async_dh_new(&crypto, dh_main.ev, tp);
  llarp_threadpool_start(tp);

  dh_main.num = 1000000;
  dh_main.completed = 0;
  struct llarp_keypair ourkey;
  struct llarp_keypair theirkey;

  crypto.keygen(&ourkey);
  crypto.keygen(&theirkey);

  llarp_tunnel_nounce_t nounce;
  llarp_buffer_t n_buff;
  n_buff.base = nounce;
  n_buff.cur = n_buff.base;
  n_buff.sz = sizeof(llarp_tunnel_nounce_t);

  size_t sz = dh_main.num;
  printf("starting %ld dh jobs\n", sz);
  /* do work here */
  while (sz--) {
    crypto.randomize(n_buff);
    llarp_async_client_dh(dh_main.dh, ourkey.sec, theirkey.pub, nounce,
                          handle_dh_complete, &dh_main);
  }
  printf("started %ld dh jobs\n", dh_main.num);
  llarp_ev_loop_run(dh_main.ev);

  llarp_threadpool_join(tp);
  llarp_async_dh_free(&dh_main.dh);

  llarp_ev_loop_free(&dh_main.ev);
  llarp_free_threadpool(&tp);
  printf("did %ld of %ld work\n", dh_main.completed, dh_main.num);
  return 0;
}

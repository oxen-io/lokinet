#include <llarp/crypto_async.h>
#include <llarp/mem.h>

#include <stdio.h>

struct bench_main {
  size_t completed;
  size_t num;
  size_t jobs;
  struct llarp_ev_loop *ev;
  struct llarp_async_cipher *cipher;
  struct llarp_crypto crypto;
};

static void handle_cipher_complete(struct llarp_cipher_result *res) {
  struct bench_main *m = (struct bench_main *)res->user;
  size_t sz = m->jobs;
  m->completed++;
  size_t left = m->num - m->completed;
  if (m->completed % 10000 == 0)
    printf("completed %ld and %ld left\n", m->completed, left);
  if (m->completed == m->num) {
    llarp_ev_loop_stop(m->ev);
  } else if (m->completed % sz == 0) {
    llarp_nounce_t nounce;
    while (sz--) {
      m->crypto.randbytes(nounce, sizeof(llarp_nounce_t));
      llarp_async_cipher_queue_op(m->cipher, &res->buff, nounce,
                                  handle_cipher_complete, m);
    }
  }
}

int main(int argc, char *argv[]) {
  struct bench_main b_main;
  struct llarp_threadpool *tp;

  llarp_mem_jemalloc();
  llarp_crypto_libsodium_init(&b_main.crypto);
  llarp_ev_loop_alloc(&b_main.ev);

  tp = llarp_init_threadpool(2);

  b_main.num = 10000000;
  b_main.jobs = 10000;
  b_main.completed = 0;
  llarp_sharedkey_t key;
  b_main.crypto.randbytes(key, sizeof(llarp_sharedkey_t));

  b_main.cipher = llarp_async_cipher_new(key, &b_main.crypto, b_main.ev, tp);
  llarp_threadpool_start(tp);

  llarp_nounce_t nounce;
  llarp_buffer_t n_buff;
  n_buff.base = nounce;
  n_buff.cur = n_buff.base;
  n_buff.sz = sizeof(llarp_nounce_t);
  size_t sz = b_main.jobs;
  printf("starting %ld jobs\n", sz);
  /* do work here */
  while (sz--) {
    llarp_buffer_t *msg = llarp_g_mem.alloc(sizeof(llarp_buffer_t), 8);
    msg->base = llarp_g_mem.alloc(1024, 1024);
    msg->sz = 1024;
    msg->cur = msg->base;
    b_main.crypto.randomize(*msg);
    b_main.crypto.randbytes(nounce, sizeof(llarp_nounce_t));
    llarp_async_cipher_queue_op(b_main.cipher, msg, nounce,
                                handle_cipher_complete, &b_main);
  }
  llarp_ev_loop_run(b_main.ev);

  llarp_threadpool_join(tp);
  llarp_async_cipher_free(&b_main.cipher);

  llarp_ev_loop_free(&b_main.ev);
  llarp_free_threadpool(&tp);
  printf("did %ld of %ld work\n", b_main.completed, b_main.num);
  return 0;
}

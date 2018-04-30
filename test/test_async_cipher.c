#include <llarp/crypto_async.h>
#include <llarp/mem.h>

#include <stdio.h>

struct bench_main {
  size_t completed;
  size_t num;
  size_t jobs;
  struct llarp_threadpool *pool;
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
    llarp_threadpool_stop(m->pool);
    printf("done\n");
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

  llarp_mem_stdlib();
  llarp_crypto_libsodium_init(&b_main.crypto);
  b_main.pool = llarp_init_threadpool(1);
  llarp_threadpool_start(b_main.pool);
  tp = llarp_init_threadpool(8);

  b_main.num = 5000000;
  b_main.jobs = 5000;
  b_main.completed = 0;
  llarp_sharedkey_t key;
  b_main.crypto.randbytes(key, sizeof(llarp_sharedkey_t));

  b_main.cipher = llarp_async_cipher_new(key, &b_main.crypto, b_main.pool, tp);
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
  printf("started %ld jobs\n", b_main.jobs);
  llarp_threadpool_wait(b_main.pool);
  llarp_threadpool_join(b_main.pool);
  llarp_threadpool_stop(tp);
  llarp_threadpool_join(tp);
  llarp_free_threadpool(&tp);
  llarp_free_threadpool(&b_main.pool);
  llarp_async_cipher_free(&b_main.cipher);
  printf("did %ld of %ld work\n", b_main.completed, b_main.num);
  return 0;
}



#include "lokinet.h"
#include "llarp.hpp"
#include "config/config.hpp"

struct lokinet_context
{
  std::shared_ptr<llarp::Context> impl;

  std::unique_ptr<std::thread> runner;

  lokinet_context() : impl{std::make_shared<llarp::Context>()}
  {}

  ~lokinet_context()
  {
    if (runner)
      runner->join();
  }
};

struct lokinet_context g_context
{};

extern "C"
{
  struct lokinet_context*
  lokinet_default()
  {
    return &g_context;
  }

  struct lokinet_context*
  lokinet_context_new()
  {
    return new lokinet_context{};
  }

  void
  lokinet_context_free(struct lokinet_context* ctx)
  {
    delete ctx;
  }

  void
  lokinet_context_start(struct lokinet_context* ctx)
  {
    ctx->runner = std::make_unique<std::thread>([ctx]() {
      auto config = std::make_shared<llarp::Config>(fs::path{""});
      ctx->impl->Configure(config);
      const llarp::RuntimeOptions opts{};
      ctx->impl->Setup(opts);
    });
  }

  void
  lokinet_context_stop(struct lokinet_context* ctx)
  {
    ctx->impl->CloseAsync();
    ctx->impl->Wait();
  }
}

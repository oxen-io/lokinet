#include <ev/vpnio.hpp>
#include <llarp.hpp>
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>

void
llarp_vpn_io_impl::AsyncClose()
{
  reader.queue.disable();
  writer.queue.disable();
  CallSafe(std::bind(&llarp_vpn_io_impl::Expunge, this));
}

void
llarp_vpn_io_impl::CallSafe(std::function<void(void)> f)
{
  auto ctx = llarp::Context::Get(ptr);
  if (ctx && ctx->CallSafe(f))
    return;
  else if (ctx == nullptr || ctx->logic == nullptr)
    f();
}

void
llarp_vpn_io_impl::Expunge()
{
  parent->impl = nullptr;
  if (parent->closed)
    parent->closed(parent);
  delete this;
}
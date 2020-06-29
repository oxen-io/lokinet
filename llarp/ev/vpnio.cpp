#include <ev/vpnio.hpp>
#include <llarp.hpp>
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>

void
llarp_vpn_io_impl::AsyncClose()
{
  reader.queue.disable();
  writer.queue.disable();

  // TODO: call asynchronously
  if (ctx)
    ctx->CallSafe([this]() { Expunge(); });
  else
    Expunge();
}

void
llarp_vpn_io_impl::Expunge()
{
  parent->impl = nullptr;
  if (parent->closed)
    parent->closed(parent);
  delete this;
}

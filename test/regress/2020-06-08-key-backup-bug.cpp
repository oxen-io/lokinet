#include <llarp.h>
#include <llarp.hpp>
#include <config/config.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <catch2/catch.hpp>

static const fs::path keyfilePath = "2020-06-08-key-backup-regression-test.private";

static llarp_main*
make_context()
{
  auto config = llarp_default_config();
  config->impl.network.m_endpointType = "null";
  config->impl.network.m_keyfile = keyfilePath;
  config->impl.bootstrap.skipBootstrap = true;
  auto ptr = llarp_main_init_from_config(config, false);
  llarp_config_free(config);
  return ptr;
}

TEST_CASE("key backup bug regression test", "[regress]")
{
  llarp::service::Address endpointAddress{};
  for (size_t index = 0; index < 10; index++)
  {
    auto context = make_context();
    REQUIRE(llarp_main_setup(context, false) == 0);
    auto ctx = llarp::Context::Get(context);
    ctx->CallSafe([ctx, index, &endpointAddress]() {
      auto ep = ctx->router->hiddenServiceContext().GetDefault();
      REQUIRE(ep != nullptr);
      if (index == 0)
      {
        // first iteration, we are getting our identity
        endpointAddress = ep->GetIdentity().pub.Addr();
        REQUIRE(not endpointAddress.IsZero());
      }
      else
      {
        REQUIRE(not endpointAddress.IsZero());
        // after the first iteration we expect the keys to stay the same
        REQUIRE(endpointAddress == ep->GetIdentity().pub.Addr());
      }
      ctx->CloseAsync();
    });
    REQUIRE(llarp_main_run(context, llarp_main_runtime_opts{}) == 0);
    llarp_main_free(context);
  }
  fs::remove(keyfilePath);
}

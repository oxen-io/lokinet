#include <llarp.h>
#include <llarp.hpp>
#include <config/config.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <catch2/catch.hpp>

static llarp_main*
make_context(fs::path keyfile)
{
  auto config = llarp_default_config();
  config->impl.network.m_endpointType = "null";
  config->impl.network.m_keyfile = keyfile;
  config->impl.bootstrap.skipBootstrap = true;
  auto ptr = llarp_main_init_from_config(config, false);
  llarp_config_free(config);
  return ptr;
}

TEST_CASE("key backup bug regression test", "[regress]")
{
  llarp::LogSilencer shutup;
  for (const fs::path& path : {"regress-1.private", "regress-2.private", ""})
  {
    llarp::service::Address endpointAddress{};
    for (size_t index = 0; index < 10; index++)
    {
      auto context = make_context(path);
      REQUIRE(llarp_main_setup(context, false) == 0);
      auto ctx = llarp::Context::Get(context);
      ctx->CallSafe([ctx, index, &endpointAddress, &path]() {
        auto ep = ctx->router->hiddenServiceContext().GetDefault();
        REQUIRE(ep != nullptr);
        if (index == 0)
        {
          REQUIRE(endpointAddress.IsZero());
          // first iteration, we are getting our identity
          endpointAddress = ep->GetIdentity().pub.Addr();
          REQUIRE(not endpointAddress.IsZero());
        }
        else
        {
          REQUIRE(not endpointAddress.IsZero());
          if (path.empty())
          {
            // we want the keys to shift
            REQUIRE(endpointAddress != ep->GetIdentity().pub.Addr());
          }
          else
          {
            // after the first iteration we expect the keys to stay the same
            REQUIRE(endpointAddress == ep->GetIdentity().pub.Addr());
          }
        }
        ctx->CloseAsync();
      });
      REQUIRE(llarp_main_run(context, llarp_main_runtime_opts{}) == 0);
      llarp_main_free(context);
    }
    fs::remove(path);
  }
}

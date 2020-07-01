#include <llarp.h>
#include <llarp.hpp>
#include <config/config.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <catch2/catch.hpp>

llarp::RuntimeOptions opts = {false, false, false};

/// make a llarp_main* with 1 endpoint that specifies a keyfile
static std::shared_ptr<llarp::Context>
make_context(std::optional<fs::path> keyfile)
{
  auto context = std::make_shared<llarp::Context>();
  context->Configure(opts, {}, {});

  context->config->network.m_endpointType = "null";
  context->config->network.m_keyfile = keyfile;
  context->config->bootstrap.skipBootstrap = true;
  context->config->api.m_enableRPCServer = false;

  return context;
}

/// test that we dont back up all keys when self.signed is missing or invalid as client
TEST_CASE("key backup bug regression test", "[regress]")
{
  // kill logging, this code is noisy
  llarp::LogSilencer shutup;
  // test 2 explicitly provided keyfiles, empty keyfile and no keyfile
  for (std::optional<fs::path> path : {std::optional<fs::path>{"regress-1.private"},
                                       std::optional<fs::path>{"regress-2.private"},
                                       std::optional<fs::path>{""},
                                       {std::nullopt}})
  {
    llarp::service::Address endpointAddress{};
    // try 10 start up and shut downs and see if our key changes or not
    for (size_t index = 0; index < 10; index++)
    {
      auto ctx = make_context(path);
      REQUIRE_NOTHROW(ctx->Setup(opts));
      ctx->CallSafe([ctx, index, &endpointAddress, &path]() {
        auto ep = ctx->router->hiddenServiceContext().GetDefault();
        REQUIRE(ep != nullptr);
        if (index == 0)
        {
          REQUIRE(endpointAddress.IsZero());
          // first iteration, we are getting our identity that we start with
          endpointAddress = ep->GetIdentity().pub.Addr();
          REQUIRE(not endpointAddress.IsZero());
        }
        else
        {
          REQUIRE(not endpointAddress.IsZero());
          if (path.has_value() and not path->empty())
          {
            // we have a keyfile provided
            // after the first iteration we expect the keys to stay the same
            REQUIRE(endpointAddress == ep->GetIdentity().pub.Addr());
          }
          else
          {
            // we want the keys to shift because no keyfile was provided
            REQUIRE(endpointAddress != ep->GetIdentity().pub.Addr());
          }
        }
        // close the router right away
        ctx->router->Die();
      });
      REQUIRE(ctx->Run({}) == 0);
      ctx.reset();
    }
    // remove keys if provied
    if (path.has_value() and not path->empty())
      fs::remove(*path);
  }
}

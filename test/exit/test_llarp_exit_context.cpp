#include <config/config.hpp>
#include <router/router.hpp>
#include <exit/context.hpp>
#include <crypto/types.hpp>
#include <llarp.h>
#include <llarp.hpp>
#include <catch2/catch.hpp>

static const llarp::RuntimeOptions opts = {.background = false, .debug = false, .isRouter = true};

std::shared_ptr<llarp::Context>
make_context()
{
  auto context = std::make_shared<llarp::Context>();
  context->Configure(opts, {}, {});
  REQUIRE(context->config != nullptr);
  REQUIRE(context->config->LoadDefault(true, fs::current_path()));

  // set testing defaults
  context->config->network.m_endpointType = "null";
  context->config->bootstrap.skipBootstrap = true;
  context->config->api.m_enableRPCServer = false;
  // make a fake inbound link
  context->config->links.m_InboundLinks.emplace_back();
  auto& link = context->config->links.m_InboundLinks.back();
  link.interface = llarp::net::LoopbackInterfaceName();
  link.addressFamily = AF_INET;
  link.port = 0;
  // configure
  return context;
}

TEST_CASE("ensure snode address allocation", "[snode]")
{
  llarp::LogSilencer shutup;
  auto ctx = make_context();
  REQUIRE_NOTHROW(ctx->Setup(opts));
  ctx->CallSafe([ctx]() {
    REQUIRE(ctx->router->IsServiceNode());
    auto& context = ctx->router->exitContext();
    llarp::PubKey pk;
    pk.Randomize();

    llarp::PathID_t firstPath, secondPath;
    firstPath.Randomize();
    secondPath.Randomize();

    REQUIRE(context.ObtainNewExit(pk, firstPath, false));
    REQUIRE(context.ObtainNewExit(pk, secondPath, false));
    REQUIRE(
        context.FindEndpointForPath(firstPath)->LocalIP()
        == context.FindEndpointForPath(secondPath)->LocalIP());
    ctx->CloseAsync();
  });
  REQUIRE(ctx->Run(opts) == 0);

  ctx.reset();
}

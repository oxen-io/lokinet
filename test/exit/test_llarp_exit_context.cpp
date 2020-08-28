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
  llarp::Config conf{};
  conf.LoadDefault(true, fs::current_path());

  // set testing defaults
  conf.network.m_endpointType = "null";
  conf.bootstrap.skipBootstrap = true;
  conf.api.m_enableRPCServer = false;
  conf.router.m_enablePeerStats = true;
  conf.router.m_publicAddress = llarp::IpAddress("1.1.1.1");
  // make a fake inbound link
  conf.links.m_InboundLinks.emplace_back();
  auto& link = conf.links.m_InboundLinks.back();
  link.interface = "0.0.0.0";
  link.addressFamily = AF_INET;
  link.port = 0;
  // configure

  auto context = std::make_shared<llarp::Context>();
  REQUIRE_NOTHROW(context->Configure(std::move(conf)));

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

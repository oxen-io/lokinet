#include <config/config.hpp>
#include <router/router.hpp>
#include <exit/context.hpp>
#include <crypto/types.hpp>
#include <llarp.h>
#include <llarp.hpp>
#include <catch2/catch.hpp>

static llarp_main*
make_context()
{
  // make config
  auto config = llarp_default_relay_config();
  // set testing defaults
  config->impl.network.m_endpointType = "null";
  config->impl.bootstrap.skipBootstrap = true;
  config->impl.api.m_enableRPCServer = false;
  // make a fake inbound link
  config->impl.links.m_InboundLinks.emplace_back();
  auto& link = config->impl.links.m_InboundLinks.back();
  link.interface = llarp::net::LoopbackInterfaceName();
  link.addressFamily = AF_INET;
  link.port = 0;
  // configure
  auto ptr = llarp_main_init_from_config(config, true);
  llarp_config_free(config);
  return ptr;
}

TEST_CASE("ensure snode address allocation", "[snode]")
{
  llarp::LogSilencer shutup;
  auto ctx = make_context();
  REQUIRE(llarp_main_setup(ctx, true) == 0);
  auto ctx_pp = llarp::Context::Get(ctx);
  ctx_pp->CallSafe([ctx_pp]() {
    REQUIRE(ctx_pp->router->IsServiceNode());
    auto& context = ctx_pp->router->exitContext();
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
    ctx_pp->CloseAsync();
  });
  REQUIRE(llarp_main_run(ctx, llarp_main_runtime_opts{.isRelay = true}) == 0);
  llarp_main_free(ctx);
}

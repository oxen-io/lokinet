// Test utility to bind a local tcp socket listener with liblokinet

#include <lokinet.h>

#include <signal.h>

#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>

bool _run{true};

void
signal_handler(int)
{
  _run = false;
}

int
main(int argc, char* argv[])
{
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  if (auto* loglevel = getenv("LOKINET_LOG"))
    lokinet_log_level(loglevel);
  else
    lokinet_log_level("info");

  std::cout << "starting up\n";

  auto shared_ctx = std::shared_ptr<lokinet_context>(lokinet_context_new(), lokinet_context_free);
  auto* ctx = shared_ctx.get();
  if (lokinet_context_start(ctx))
    throw std::runtime_error{"could not start context"};

  int status;
  for (status = lokinet_status(ctx); _run and status == -1; status = lokinet_status(ctx))
  {
    std::cout << "waiting for lokinet to be ready..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  }
  if (not _run)
  {
    std::cout << "exit requested before context was ready.\n";
    return 0;
  }
  if (status != 0)
  {
    std::cout << "lokinet_status = " << status << " after waiting for ready.\n";
    return 0;
  }

  // wait a bit just so log output calms down so we can see stuff
  // printed from here
  std::this_thread::sleep_for(std::chrono::milliseconds{3000});

  const auto port = 10000;

  auto id = lokinet_inbound_stream(port, ctx);
  if (id < 0)
  {
    std::cout << "failed to bind tcp socket\n";
    return 0;
  }

  std::cout << "bound tcp on localhost port: " << port << "\n";

  auto addr_c = lokinet_address(ctx);
  std::string addr{addr_c};
  free(addr_c);

  std::cout << "lokinet address: " << addr << "\n";

  size_t counter = 0;
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    if (++counter % 30 == 0)
      std::cout << "lokinet address: " << addr << "\n";
  } while (_run);

  std::cout << "tcp_listen shutting down...\n";

  lokinet_close_stream(id, ctx);
  return 0;
}


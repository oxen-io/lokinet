// Test utility to bind a local tcp socket listener with liblokinet

#include <lokinet.h>

#include <llarp/util/logging.hpp>

#include <csignal>

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

bool run{true};

void
signal_handler(int)
{
  run = false;
  int a;
}

int
main(int argc, char* argv[])
{
  if (argc < 3 || argc > 4)
  {
    std::cerr << "Usage: " << argv[0] << " something.{loki,snode} port [testnet]\n";
    return 0;
  }

  std::string netid = "lokinet";
  std::string data_dir = "./tcp_connect_data_dir";

  if (argc == 4) // if testnet
  {
    netid = "gamma"; // testnet netid
    data_dir += "/testnet";
  }

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  if (auto* loglevel = getenv("LOKINET_LOG"))
    lokinet_log_level(loglevel);
  else
    lokinet_log_level("info");

  std::cerr << "starting up\n";

  lokinet_set_netid(netid.c_str());
  auto shared_ctx = std::shared_ptr<lokinet_context>(lokinet_context_new(), lokinet_context_free);
  auto* ctx = shared_ctx.get();
  lokinet_set_data_dir(data_dir.c_str(), ctx);
  if (lokinet_context_start(ctx))
    throw std::runtime_error{"could not start context"};

  int status;
  for (status = lokinet_status(ctx); run and status == -1; status = lokinet_status(ctx))
  {
    std::cerr << "waiting for lokinet to be ready..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  }
  if (not run)
  {
    std::cerr << "exit requested before context was ready.\n";
    return 0;
  }
  if (status != 0)
  {
    std::cerr << "lokinet_status = " << status << " after waiting for ready.\n";
    return 0;
  }

  if (auto* loglevel = getenv("QUIC_LOG"))
    llarp::log::set_level("quic", llarp::log::level_from_string(loglevel));
  else
    llarp::log::set_level("quic", llarp::log::Level::trace);

  std::cerr << "\n\nquic log level: " << llarp::log::to_string(llarp::log::get_level("quic")) << "\n\n";

  auto addr_c = lokinet_address(ctx);
  std::string addr{addr_c};
  free(addr_c);
  std::cerr << "lokinet address: " << addr << "\n";

  lokinet_stream_result stream_res;

  std::string target{argv[1]};
  target = target + ":" + argv[2];

  // hard-coded IP:port for liblokinet to bind to
  //  connect via tcp will stream traffic to whatever
  lokinet_outbound_stream(&stream_res, target.c_str(), "127.0.0.1:54321", ctx);

  if (stream_res.error)
  {
    std::cerr << "failed to prepare outbound tcp: " << strerror(stream_res.error) << "\n";
    return 0;
  }

  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  } while (run);

  std::cerr << "tcp_connect shutting down...\n";

  lokinet_close_stream(stream_res.stream_id, ctx);
  return 0;
}


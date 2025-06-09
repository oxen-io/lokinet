#pragma once

#include <memory>
#include <thread>
#include <cstdint>

namespace llarp
{
  struct Context;
  struct Config;
}  // namespace llarp

namespace lokinet
{
  class Lokinet
  {
    std::shared_ptr<llarp::Context> context;
    std::shared_ptr<llarp::Config> config;

    std::thread run_thread{};

    public:

    Lokinet(const std::string& net_id = "testnet");

    ~Lokinet();

    // only applies if called before run()
    void set_log_level(const std::string& level);

    void run();

    uint16_t udp_session(const std::string& remote, uint16_t port);
  };
}  // namespace lokinet

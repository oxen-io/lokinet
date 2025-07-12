#include <lokinet.hpp>

#include <thread>
#include <iostream>
#include <cstdint>
#include <filesystem>

using namespace std::literals;

int main(int argc, char** argv) {
  lokinet::Lokinet loki{std::filesystem::path{"lokinet.ini"}};
  //lokinet::Lokinet loki{lokinet::Network::TESTNET};
  std::this_thread::sleep_for(5s);
  std::string ignored;
  if (argc > 1) {
    std::string target{argv[1]};
    std::cout << "\nPRESS ENTER TO START SESSION TO " << target << "\n";
    std::getline(std::cin, ignored);
    try {
      auto udp_info = loki.establish_udp_blocking(target, 12345);
      std::cout << "\nudp bound to port " << udp_info.local_port << "\n";
    }
    catch (const std::exception& e) {
      std::cerr << "\nError establishing session to " << target << ": " << e.what() << "\n";
      return 1;
    }

    /*
    loki.map_tcp_remote_port(std::string{argv[1]}, 12345,
        [&](auto tunnel_info) {
          std::cout << "\n\nTCP bound to port " << tunnel_info.local_port << "\n\n";
        },
        [&](auto error_str) {
          std::cerr << "\nTCP Tunnel map error: " << error_str << "\n";
        });
    */
  }
  std::cout << "\nPRESS ENTER TO EXIT\n";
  std::getline(std::cin, ignored);
  std::cout << "\nEXITING\n";
}

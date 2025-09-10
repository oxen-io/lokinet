#include <lokinet.hpp>

#include <exception>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>

using namespace std::literals;

int main(int argc, char** argv)
{
    if (argc <= 1)
    {
        std::cerr << "USAGE: " << argv[0] << " {WHATEVER.loki | WHATEVER.snode}\n";
        return 1;
    }

    std::string target{argv[1]};

    lokinet::Lokinet loki{std::filesystem::path{"lokinet.ini"}};

    std::promise<void> prom;
    loki.on_connected([&] {
        std::cout << "\n\x1b[32;1mLokinet connected!\x1b[0m\n\n\x1b[33;1mINITIATING SESSION TO " << target
                  << "\x1b[0m\n\n"
                  << std::flush;
        loki.establish_udp(
            target,
            12345,
            [](auto udp_info) {
                std::cout << "\n\x1b[32;1mUDP bound to port " << udp_info.local_port << "\x1b[0m\n\n" << std::flush;
            },
            [&prom](std::string_view fail_msg) {
                try
                {
                    throw std::runtime_error{std::string{fail_msg}};
                }
                catch (...)
                {
                    prom.set_exception(std::current_exception());
                }
            });
    });
    try
    {
        prom.get_future().get();
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n\n\x1b[31;1mError establishing session to " << target << ": " << e.what() << "\x1b[0m\n\n";
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
    std::cout << "\nPRESS ENTER TO EXIT\n";
    std::string ignored;
    std::getline(std::cin, ignored);
    std::cout << "\nEXITING\n";
}

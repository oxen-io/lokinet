#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <util/logging/logger.hpp>

#ifdef _WIN32
#include <winsock2.h>
int
startWinsock()
{
  WSADATA wsockd;
  int err;
  err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
  if (err)
  {
    perror("Failed to start Windows Sockets");
    return err;
  }
  return 0;
}
#endif

int
main(int argc, char* argv[])
{
  llarp::LogSilencer shutup{};

#ifdef _WIN32
  if (startWinsock())
    return -1;
#endif

  int result = Catch::Session().run(argc, argv);
#ifdef _WIN32
  WSACleanup();
#endif
  return result;
}

#include <gtest/gtest.h>

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
main(int argc, char** argv)
{
  llarp::LogSilencer shutup;
#ifdef _WIN32
  if (startWinsock())
    return -1;
#endif

  ::testing::InitGoogleTest(&argc, argv);
  int r = RUN_ALL_TESTS();
#ifdef _WIN32
  WSACleanup();
#endif
  return r;
}

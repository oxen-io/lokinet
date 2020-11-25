#pragma once

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

namespace llarp::vpn
{
  struct IOCTL
  {
    const int _fd;

    IOCTL() : _fd{::socket(AF_INET, SOCK_DGRAM, 0)}
    {
      if (_fd == -1)
        throw std::invalid_argument{strerror(errno)};
    };

    ~IOCTL()
    {
      ::close(_fd);
    }

    template <typename Command, typename... Args>
    void
    ioctl(Command cmd, Args&&... args)
    {
      if (::ioctl(_fd, cmd, std::forward<Args>(args)...) == -1)
        throw std::runtime_error("ioctl failed: " + std::string{strerror(errno)});
    }
  };
}  // namespace llarp::vpn

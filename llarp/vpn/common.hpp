#pragma once

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cerrno>

namespace llarp::vpn
{
  class permission_error : public std::runtime_error
  {
   public:
    using std::runtime_error::runtime_error;
  };

  struct IOCTL
  {
    const int _fd;

    explicit IOCTL(int af) : _fd{::socket(af, SOCK_DGRAM, IPPROTO_IP)}
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
      {
        if (errno == EACCES)
        {
          throw permission_error{"we are not allowed to call this ioctl"};
        }
        else
          throw std::runtime_error("ioctl failed: " + std::string{strerror(errno)});
      }
    }
  };
}  // namespace llarp::vpn

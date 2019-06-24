#include <util/fs.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <system_error>
#include <util/logger.hpp>

namespace llarp
{
  namespace util
  {
    static std::error_code
    errno_error()
    {
      int e = errno;
      errno = 0;
      return std::make_error_code(static_cast< std::errc >(e));
    }

    error_code_t
    EnsurePrivateFile(fs::path pathname)
    {
      const auto str  = pathname.string();
      error_code_t ec = errno_error();
      if(fs::exists(pathname, ec))  // file exists
      {
        fs::permissions(pathname,
                        ~fs::perms::group_all | ~fs::perms::others_all
                            | fs::perms::owner_read | fs::perms::owner_write,
                        ec);
      }
      else if(!ec)  // file is not there
      {
        errno  = 0;
        int fd = ::open(str.c_str(), O_RDWR | O_CREAT, 0600);
        ec     = errno_error();
        if(fd != -1)
        {
          ::close(fd);
        }
      }
      if(ec)
        llarp::LogError("failed to ensure ", str, ", ", ec.message());
      return ec;
    }
  }  // namespace util
}  // namespace llarp
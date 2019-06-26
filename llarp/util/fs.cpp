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
      errno  = 0;
      error_code_t ec = errno_error();
      if(fs::exists(pathname, ec))  // file exists
      {
        auto st = fs::status(pathname, ec);
        if(ec)
          return ec;
        auto perms = st.permissions();
        if((perms & fs::perms::others_exec) != fs::perms::none)
          perms ^= fs::perms::others_exec;
        if((perms & fs::perms::others_write)  != fs::perms::none)
          perms ^= fs::perms::others_write;
        if((perms & fs::perms::others_write)  != fs::perms::none)
          perms ^= fs::perms::others_write;
        if((perms & fs::perms::group_read)  != fs::perms::none)
          perms ^= fs::perms::group_read;
        if((perms & fs::perms::others_read)  != fs::perms::none)
          perms ^= fs::perms::others_read;
        if((perms & fs::perms::owner_exec) != fs::perms::none)
          perms ^= fs::perms::owner_exec;
        fs::permissions(pathname, perms, ec);
        if(ec)
          llarp::LogError("failed to set permissions on ", pathname);
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
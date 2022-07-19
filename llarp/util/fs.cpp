#include "fs.hpp"

#include <llarp/util/logging.hpp>
#include <llarp/util/formattable.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace llarp
{
  namespace util
  {
    static std::error_code
    errno_error()
    {
      int e = errno;
      errno = 0;
      return std::make_error_code(static_cast<std::errc>(e));
    }

    error_code_t
    EnsurePrivateFile(fs::path pathname)
    {
      errno = 0;
      error_code_t ec = errno_error();
      const auto str = pathname.string();
      if (fs::exists(pathname, ec))  // file exists
      {
        auto st = fs::status(pathname);
        auto perms = st.permissions();
        if ((perms & fs::perms::others_exec) != fs::perms::none)
          perms = perms ^ fs::perms::others_exec;
        if ((perms & fs::perms::others_write) != fs::perms::none)
          perms = perms ^ fs::perms::others_write;
        if ((perms & fs::perms::others_write) != fs::perms::none)
          perms = perms ^ fs::perms::others_write;
        if ((perms & fs::perms::group_read) != fs::perms::none)
          perms = perms ^ fs::perms::group_read;
        if ((perms & fs::perms::others_read) != fs::perms::none)
          perms = perms ^ fs::perms::others_read;
        if ((perms & fs::perms::owner_exec) != fs::perms::none)
          perms = perms ^ fs::perms::owner_exec;

        fs::permissions(pathname, perms, ec);
        if (ec)
          llarp::LogError("failed to set permissions on ", pathname);
      }
      else  // file is not there
      {
        errno = 0;
        int fd = ::open(str.c_str(), O_RDWR | O_CREAT, 0600);
        ec = errno_error();
        if (fd != -1)
        {
          ::close(fd);
        }
      }

#ifndef WIN32
      if (ec)
        llarp::LogError("failed to ensure ", str, ", ", ec.message());
      return ec;
#else
      return {};
#endif
    }
  }  // namespace util
}  // namespace llarp

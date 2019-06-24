#include <util/fs.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <system_error>

namespace llarp
{
  namespace util
  {
    error_code_t
    EnsurePrivateFile(fs::path pathname)
    {
      auto str = pathname.string();
      error_code_t ec;
      if(fs::exists(pathname, ec))  // file exists
      {
        fs::permissions(pathname,
                        ~fs::perms::group_all | ~fs::perms::others_all
                            | fs::perms::owner_read | fs::perms::owner_write,
                        ec);
      }
      else if(!ec)  // file is not there
      {
        int fd = ::open(str.c_str(), O_WRONLY | O_CREAT, 0600);
        int e  = errno;
        if(fd != -1)
        {
          ::close(fd);
        }
        ec    = std::error_code(e, std::generic_category());
        errno = 0;
      }
      return ec;
    }
  }  // namespace util
}  // namespace llarp
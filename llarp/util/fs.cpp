#include "fs.hpp"

#include <llarp/util/logging/logger.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace cpp17
{
  namespace filesystem
  {
#ifdef LOKINET_USE_CPPBACKPORT
    const fs::perms active_bits(
        fs::perms::all | fs::perms::set_uid | fs::perms::set_gid | fs::perms::sticky_bit);
    inline mode_t
    mode_cast(fs::perms prms)
    {
      return prms & active_bits;
    }

    void
    permissions(const fs::path& p, fs::perms prms, std::error_code& ec)
    {
      std::error_code local_ec;

      // OS X <10.10, iOS <8.0 and some other platforms don't support
      // fchmodat(). Solaris (SunPro and gcc) only support fchmodat() on
      // Solaris 11 and higher, and a runtime check is too much trouble. Linux
      // does not support permissions on symbolic links and has no plans to
      // support them in the future.  The chmod() code is thus more practical,
      // rather than always hitting ENOTSUP when sending in
      // AT_SYMLINK_NO_FOLLOW.
      //  - See the 3rd paragraph of
      // "Symbolic link ownership, permissions, and timestamps" at:
      //   "http://man7.org/linux/man-pages/man7/symlink.7.html"
      //  - See the fchmodat() Linux man page:
      //   "http://man7.org/linux/man-pages/man2/fchmodat.2.html"
#if defined(AT_FDCWD) && defined(AT_SYMLINK_NOFOLLOW)                                           \
    && !(defined(__SUNPRO_CC) || defined(__sun) || defined(sun))                                \
    && !(defined(linux) || defined(__linux) || defined(__linux__))                              \
    && !(defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000)  \
    && !(defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 80000) \
    && !(defined(__QNX__) && (_NTO_VERSION <= 700))
      if (::fchmodat(AT_FDCWD, p.c_str(), mode_cast(prms), 0))
#else  // fallback if fchmodat() not supported
      if (::chmod(p.c_str(), mode_cast(prms)))
#endif
      {
        const int err = errno;
        ec.assign(err, std::generic_category());
      }
    }
#endif
  }  // namespace filesystem
}  // namespace cpp17

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

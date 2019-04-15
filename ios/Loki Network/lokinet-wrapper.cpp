#include <dirent.h>
#include <syslog.h>
#include <logger.hpp>
#include <llarp.h>

// MARK: - dirent.h workaround
// The opendir$INODE64, readdir$INODE64 and readdir_r$INODE64 functions below are wrapped as a workaround for
// an issue where Xcode can't find these functions during linking when targeting iOS.

extern "C"
DIR *
opendir$INODE64(char *dirName)
{
    return opendir(dirName);
}

extern "C"
struct dirent *
readdir$INODE64(DIR *dir)
{
    return readdir(dir);
}

extern "C"
int
readdir_r$INODE64(DIR *dir, struct dirent *entry, struct dirent **result)
{
    return readdir_r(dir, entry, result);
}

// MARK: - syslog.h workaround
// The syslog$DARWIN_EXTSN function below is wrapped as a workaround for an issue where Xcode can't find
// this function during linking when targetion iOS.

extern "C"
void
syslog$DARWIN_EXTSN(int priority, const char *format, va_list ap)
{
    syslog(priority, format, ap);
}

// MARK: - LLARP

extern "C"
void
_llarp_enable_debug_mode() {
    llarp::SetLogLevel(llarp::eLogDebug);
}

extern "C"
bool
_llarp_ensure_config(const char *fname, const char *baseDir, bool overwrite, bool asRouter)
{
    return llarp_ensure_config(fname, baseDir, overwrite, asRouter);
}

extern "C"
struct llarp_main *
_llarp_main_init(const char *fname, bool multiProcess)
{
    return llarp_main_init(fname, multiProcess);
}

extern "C"
int
_llarp_main_setup(struct llarp_main *ptr)
{
    return llarp_main_setup(ptr);
}

extern "C"
int
_llarp_main_run(struct llarp_main *ptr) {
    return llarp_main_run(ptr);
}

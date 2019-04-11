#include <dirent.h>
#include <logger.hpp>
#include <llarp.h>
#include <router.hpp>

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
readdir_r$INODE64(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    return readdir_r(dirp, entry, result);
}

// MARK: - LLARP

extern "C"
void
_enable_debug_mode() {
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

// MARK: - Router

extern "C"
bool
_llarp_find_or_create_encryption_file(const char *fpath) {
    llarp::Crypto crypt(llarp::Crypto::sodium{});
    return llarp_findOrCreateEncryption(crypt, fpath, encryption);
}

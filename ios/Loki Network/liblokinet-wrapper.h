#include <dirent.h>
#include <stdbool.h>

// MARK: - dirent.h workaround
// The opendir$INODE64, readdir$INODE64 and readdir_r$INODE64 functions below are wrapped as a workaround for
// an issue where Xcode can't find these functions during linking when targeting iOS.

DIR *
opendir$INODE64(char *dirName);

struct dirent *
readdir$INODE64(DIR *dir);

int
readdir_r$INODE64(DIR *dir, struct dirent *entry, struct dirent **result);

// MARK: - syslog.h workaround
// The syslog$DARWIN_EXTSN function below is wrapped as a workaround for an issue where Xcode can't find
// this function during linking when targetion iOS.

void
syslog$DARWIN_EXTSN(int priority, const char *format, va_list ap);

// MARK: - LLARP

void
_llarp_enable_debug_mode();

bool
_llarp_ensure_config(const char *fname, const char *baseDir, bool overwrite, bool asRouter);

struct llarp_main *
_llarp_main_init(const char *fname, bool multiProcess);

int
_llarp_main_setup(struct llarp_main *ptr);

int
_llarp_main_run(struct llarp_main *ptr);

#include "llarp.h"

extern "C"
bool
_llarp_ensure_config(const char *fname, const char *basedir, bool overwrite, bool asRouter)
{
    return llarp_ensure_config(fname, basedir, overwrite, asRouter);
}

extern "C"
struct llarp_main *
_llarp_main_init(const char *fname, bool multiProcess)
{
    return llarp_main_init(fname, multiProcess);
}

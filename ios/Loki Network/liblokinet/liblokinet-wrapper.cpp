#include "llarp.h"

extern "C"
bool
_llarp_ensure_config(const char *fname, const char *basedir, bool overwrite,
                     bool asrouter)
{
    return llarp_ensure_config(fname, basedir, overwrite, asrouter);
}

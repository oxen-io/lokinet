#ifndef LLARP_VERSION_H
#define LLARP_VERSION_H

#ifndef LLARP_VERSION_MAJ
#define LLARP_VERSION_MAJ "0"
#endif

#ifndef LLARP_VERSION_MIN
#define LLARP_VERSION_MIN "0"
#endif

#ifndef LLARP_VERSION_PATCH
#define LLARP_VERSION_PATCH "0"
#endif

#ifndef LLARP_VERSION_NUM
#ifdef GIT_REV
#define LLARP_VERSION_NUM                                             \
  "-" LLARP_VERSION_MAJ "." LLARP_VERSION_MIN "." LLARP_VERSION_PATCH \
  "-" GIT_REV
#else
#define LLARP_VERSION_NUM \
  "-" LLARP_VERSION_MAJ "." LLARP_VERSION_MIN "." LLARP_VERSION_PATCH
#endif
#endif
#define LLARP_VERSION "llarpd" LLARP_VERSION_NUM

#ifndef LLARP_RELEASE_MOTTO
#define LLARP_RELEASE_MOTTO "(dev build)"
#endif
#endif

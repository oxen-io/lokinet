#ifndef LLARP_VERSION_HPP
#define LLARP_VERSION_HPP

#ifndef LLARP_VERSION_MAJ
#define LLARP_VERSION_MAJ 0
#endif

#ifndef LLARP_VERSION_MIN
#define LLARP_VERSION_MIN 6
#endif

#ifndef LLARP_VERSION_PATCH
#define LLARP_VERSION_PATCH 0
#endif

#ifndef LLARP_VERSION_NUM
#ifdef GIT_REV
#define LLARP_VERSION_NUM                                    \
  "-LLARP_VERSION_MAJ.LLARP_VERSION_MIN.LLARP_VERSION_PATCH" \
  "-" GIT_REV
#else
#define LLARP_VERSION_NUM \
  "-LLARP_VERSION_MAJ.LLARP_VERSION_MIN.LLARP_VERSION_PATCH"
#endif
#endif

#if defined(_WIN32) && defined(RC_INVOKED)
#define LLARP_VERSION \
  LLARP_VERSION_MAJ, LLARP_VERSION_MIN, LLARP_VERSION_PATCH, 0
#else
#define LLARP_VERSION "lokinet" LLARP_VERSION_NUM
#endif

#ifndef LLARP_RELEASE_MOTTO
#define LLARP_RELEASE_MOTTO "(dev build)"
#endif

struct Version
{
  static const char LLARP_NET_ID[];
};
#endif

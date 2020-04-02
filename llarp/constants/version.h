#pragma once

// Don't include this file directly but rather go through version.hpp instead.
// This is only here so version.cpp.in and the weird archaic windows build
// recipies can use the version.

#define LLARP_NAME "lokinet"

#define LLARP_VERSION_MAJ 0
#define LLARP_VERSION_MIN 7
#define LLARP_VERSION_PATCH 0

#define LLARP_DEFAULT_NETID "lokinet"

#ifndef LLARP_RELEASE_MOTTO
#define LLARP_RELEASE_MOTTO "(dev build)"
#endif

#if defined(_WIN32) && defined(RC_INVOKED)
#define LLARP_VERSION \
  LLARP_VERSION_MAJOR, LLARP_VERSION_MINOR, LLARP_VERSION_PATCH, 0

#define MAKE_TRIPLET(X, Y, Z) TRIPLET_CAT(X, ., Y, ., Z)
#define TRIPLET_CAT(X, D1, Y, D2, Z) X##D1##Y##D2##Z

#define LLARP_VERSION_TRIPLET \
  MAKE_TRIPLET(LLARP_VERSION_MAJOR, LLARP_VERSION_MINOR, LLARP_VERSION_PATCH)

#endif

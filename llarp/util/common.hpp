#pragma once
#ifdef __STRICT_ANSI__
#define INLINE __inline__
#else
#define INLINE inline
#endif

#include <cstdint>
#include <cstdlib>

// clang-format off
#if defined(__GNUC__) || defined(__clang__)
    #define DO_PRAGMA(X)                    _Pragma(#X)
    #define DISABLE_WARNING_PUSH            DO_PRAGMA(GCC diagnostics push)
    #define DISABLE_WARNING_POP             DO_PRAGMA(GCC diagnostic pop)
    #define DISABLE_WARNING(wname)          DO_PRAGMA(GCC diagnostic ignored #wname)
#elif defined(_MSC_VER_)
    #define DISABLE_WARNING_PUSH            __pragma(warning(push))
    #define DISABLE_WARNING_POP             __pragma(warning(pop))
#define DISABLE_WARNING(wnumber)            __pragma(warning(disable : wnumber))
#else
    // unknown compiler, ignore suppression directives
    #define DISABLE_WARNING_PUSH
    #define DISABLE_WARNING_POP
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define DISABLE_MAYBE_UNINITIALIZED     DISABLE_WARNING(-Wmaybe-uninitialized)
#elif defined(_MSC_VER_)
    #define DISABLE_MAYBE_UNINITIALIZED     DISABLE_WARNING(C4701)
#else
    #define DISABLE_MAYBE_UNINITIALIZED
#endif
// clang-format on

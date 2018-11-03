
#ifndef sodium_core_H
#define sodium_core_H

#include <sodium/export.h>

// optimise the bit-flipping codepaths if we're on ix86
#if defined(_WIN32) || defined(_M_IX86) || defined(_M_X64) \
    || defined(__i386__) || defined(__amd64__)
#define NATIVE_LITTLE_ENDIAN 1
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  SODIUM_EXPORT
  int
  sodium_init(void) __attribute__((warn_unused_result));

  /* ---- */

  SODIUM_EXPORT
  int
  sodium_set_misuse_handler(void (*handler)(void));

  SODIUM_EXPORT
  void
  sodium_misuse(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif

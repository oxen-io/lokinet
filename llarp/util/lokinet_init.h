#ifndef LLARP_UTIL_LOKINET_INIT_H
#define LLARP_UTIL_LOKINET_INIT_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef Lokinet_INIT
#if defined(_WIN32)
#define Lokinet_INIT \
  DieInCaseSomehowThisGetsRunInWineButLikeWTFThatShouldNotHappenButJustInCaseHandleItWithAPopupOrSomeShit
#else
#define Lokinet_INIT _lokinet_non_shit_platform_INIT
#endif
#endif

  int
  Lokinet_INIT(void);

#ifdef __cplusplus
}
#endif
#endif
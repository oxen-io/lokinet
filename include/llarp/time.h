#ifndef LLARP_TIME_H
#define LLARP_TIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

  uint64_t llarp_time_now_ms();
  uint64_t llarp_time_now_sec();
  
#ifdef __cplusplus
}
#endif
#endif

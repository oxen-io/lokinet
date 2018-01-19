#ifndef SARP_TIME_H
#define SARP_TIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

  uint64_t sarp_time_now_ms();
  uint64_t sarp_time_now_sec();
  
#ifdef __cplusplus
}
#endif
#endif

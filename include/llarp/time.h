#ifndef LLARP_TIME_H
#define LLARP_TIME_H
#include <llarp/types.h>
#ifdef __cplusplus
extern "C" {
#endif

  llarp_time_t llarp_time_now_ms();
  llarp_seconds_t llarp_time_now_sec();
  
#ifdef __cplusplus
}
#endif
#endif

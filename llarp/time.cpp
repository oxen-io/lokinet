#include <llarp/time.h>
#include <sys/time.h>

// these _should_ be 32-bit safe...
llarp_time_t
llarp_time_now_ms()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  llarp_time_t timeNow =
      (llarp_time_t)(tv.tv_sec) * 1000 + (llarp_time_t)(tv.tv_usec) / 1000;
  return timeNow;
}

llarp_seconds_t
llarp_time_now_sec()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  llarp_time_t timeNow = tv.tv_sec;
  return timeNow;
}

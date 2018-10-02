#include <llarp/time.h>
#ifdef __linux__
#include <time.h>
#else
#include <sys/time.h>
#endif

// these _should_ be 32-bit safe...
llarp_time_t
llarp_time_now_ms()
{
  struct timeval tv;
  struct timezone z;
  z.tz_minuteswest = 0;
  time_t t = time(nullptr);
  z.tz_dsttime = gmtime(&t)->tm_isdst;
  gettimeofday(&tv, &z);
  llarp_time_t timeNow =
      (llarp_time_t)(tv.tv_sec) * 1000 + (llarp_time_t)(tv.tv_usec) / 1000;
  return timeNow;
}

llarp_seconds_t
llarp_time_now_sec()
{
  struct timeval tv;
  struct timezone z;
  z.tz_minuteswest = 0;
  time_t t = time(nullptr);
  z.tz_dsttime = gmtime(&t)->tm_isdst;
  gettimeofday(&tv, &z);
  llarp_time_t timeNow = tv.tv_sec;
  return timeNow;
}

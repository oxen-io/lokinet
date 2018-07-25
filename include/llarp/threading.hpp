#ifndef LLARP_THREADING_HPP
#define LLARP_THREADING_HPP
#include <mutex>
#if defined(__MINGW32__)
#include <llarp/win32/threads/mingw.condition_variable.h>
#include <llarp/win32/threads/mingw.mutex.h>
#include <llarp/win32/threads/mingw.thread.h>
#else
#include <condition_variable>
#include <thread>
#endif
#endif
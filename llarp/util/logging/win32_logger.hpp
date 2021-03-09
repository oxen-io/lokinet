#pragma once
#if defined(_WIN32)
#include "ostream_logger.hpp"
#define VC_EXTRALEAN
#include <windows.h>

namespace llarp
{
  struct Win32LogStream : public OStreamLogStream
  {
    Win32LogStream(std::ostream& out);

    void
    PreLog(
        std::stringstream& s,
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename) const override;

    void
    PostLog(std::stringstream& s) const override;

    void Tick(llarp_time_t) override{};

    void
    Print(LogLevel lvl, const char*, const std::string& msg) override;

   private:
    std::ostream& m_Out;

    bool isConsoleModern = true;  // qol fix so oldfag clients don't see ugly escapes

    HANDLE fd1 = GetStdHandle(STD_OUTPUT_HANDLE);
  };
}  // namespace llarp
#endif

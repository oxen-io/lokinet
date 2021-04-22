#pragma once
#ifdef __linux__
namespace llarp
{
  namespace GNULinux
  {
    /// switch current process to use network namepsace by name
    /// returns true if successfully switched otherwise returns false
    bool
    NetNSSwitch(const char* name);
  }  // namespace GNULinux
}  // namespace llarp
#else
#error "Don't include this file"
#endif

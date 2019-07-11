#ifndef LLARP_HOOK_SHELL_HPP
#define LLARP_HOOK_SHELL_HPP

#include "ihook.hpp"

namespace llarp
{
  namespace hooks
  {
    /// exec file based hook notifier
    Backend_ptr
    ExecShellBackend(std::string execFilePath);
  }  // namespace hooks
}  // namespace llarp
#endif
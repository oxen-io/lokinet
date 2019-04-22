#ifndef LLARP_HOOK_IHOOK_HPP
#define LLARP_HOOK_IHOOK_HPP
#include <string>
#include <unordered_map>
#include <memory>

namespace llarp
{
  namespace hooks
  {
    /// base type for event hook handlers
    struct IBackend
    {
      ~IBackend(){};
      virtual void
      NotifyAsync(std::unordered_map< std::string, std::string > params) = 0;

      /// start backend
      virtual bool
      Start() = 0;

      /// stop backend
      virtual bool
      Stop() = 0;
    };

    using Backend_ptr = std::unique_ptr< IBackend >;
  }  // namespace hooks
}  // namespace llarp

#endif
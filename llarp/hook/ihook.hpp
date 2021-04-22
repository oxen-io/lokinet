#pragma once
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
      virtual ~IBackend() = 0;
      virtual void
      NotifyAsync(std::unordered_map<std::string, std::string> params) = 0;

      /// start backend
      virtual bool
      Start() = 0;

      /// stop backend
      virtual bool
      Stop() = 0;
    };

    using Backend_ptr = std::shared_ptr<IBackend>;

    inline IBackend::~IBackend() = default;
  }  // namespace hooks
}  // namespace llarp

#ifndef LLARP_TYPE_HELPERS_HPP
#define LLARP_TYPE_HELPERS_HPP

namespace llarp
{
  namespace util
  {
    struct NoCopy
    {
      NoCopy() = default;

      NoCopy(const NoCopy&) = delete;
      NoCopy&
      operator=(const NoCopy&) = delete;

      NoCopy(NoCopy&&) = default;
      NoCopy&
      operator=(NoCopy&&) = default;
    };

    struct NoMove : public NoCopy
    {
      NoMove() = default;

      NoMove(NoMove&&) = delete;
      NoMove&
      operator=(NoMove&&) = delete;
    };

  }  // namespace util
}  // namespace llarp

#endif

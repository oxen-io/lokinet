#pragma once

#include <llarp/util/aligned.hpp>
#include <llarp/util/formattable.hpp>

namespace llarp::layers::flow
{
  /// a flow layer tag that indicates a distinct convo between us and other entity on the flow
  /// layer. was called a convotag in the past.
  struct FlowTag : public AlignedBuffer<16>
  {
    using AlignedBuffer<16>::AlignedBuffer;

    std::string
    ToString() const;

    /// make a random flow tag
    static FlowTag
    random();
  };
}  // namespace llarp::layers::flow

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<llarp::layers::flow::FlowTag> = true;

}

namespace std
{
  template <>
  struct hash<llarp::layers::flow::FlowTag>
  {
    size_t
    operator()(const llarp::layers::flow::FlowTag& tag) const
    {
      return std::hash<llarp::AlignedBuffer<16>>{}(tag);
    }
  };
}  // namespace std

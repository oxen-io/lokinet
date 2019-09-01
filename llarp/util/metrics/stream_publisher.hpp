#ifndef LLARP_METRICS_STREAM_PUBLISHER_HPP
#define LLARP_METRICS_STREAM_PUBLISHER_HPP

#include <util/metrics/core.hpp>

#include <iosfwd>

namespace llarp
{
  namespace metrics
  {
    class StreamPublisher final : public Publisher
    {
      std::ostream& m_stream;

     public:
      StreamPublisher(std::ostream& stream) : m_stream(stream)
      {
      }

      ~StreamPublisher() override = default;

      void
      publish(const Sample& values) override;
    };
  }  // namespace metrics

}  // namespace llarp

#endif

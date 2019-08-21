#ifndef LLARP_METRICS_METRICTANK_PUBLISHER_HPP
#define LLARP_METRICS_METRICTANK_PUBLISHER_HPP

#include <util/metrics_core.hpp>

#include <util/queue.hpp>

#include <absl/types/variant.h>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace llarp
{
  namespace metrics
  {
    class MetricTankPublisherInterface : public Publisher
    {
     public:
      // Format for metrictank is <metric path, value, seconds since epoch>
      // Metric path = metrics.namespaces.metric;key=value;key1=value2
      using PublishData = std::tuple< std::string, std::string, absl::Time >;
      using Tags        = std::map< std::string, std::string >;

     private:
      const std::string m_suffix;  // tags to send to metric tank

     public:
      MetricTankPublisherInterface(const Tags& tags)
          : m_suffix(makeSuffix(tags))
      {
      }

      ~MetricTankPublisherInterface() override = default;

      static std::string
      makeSuffix(const Tags& tags);

      void
      publish(const Sample& values) override;

      virtual void
      publish(const std::vector< PublishData >& data) = 0;
    };

    class MetricTankPublisher final : public MetricTankPublisherInterface
    {
     private:
      const std::string m_host;
      const short m_port;

      struct StopStruct
      {
      };

      using Queue = thread::Queue<
          absl::variant< std::vector< PublishData >, StopStruct > >;

      Queue m_queue;         // queue of things to publish
      std::thread m_worker;  // worker thread

      void
      work();

     public:
      MetricTankPublisher(const Tags& tags, std::string host, short port)
          : MetricTankPublisherInterface(tags)
          , m_host(std::move(host))
          , m_port(port)
          , m_queue(100)
          , m_worker(&MetricTankPublisher::work, this)
      {
      }

      ~MetricTankPublisher() override
      {
        // Push back a signal value onto the queue
        m_queue.pushBack(StopStruct());
      }

      void
      publish(const std::vector< PublishData >& data) override;
    };

  }  // namespace metrics
}  // namespace llarp

#endif

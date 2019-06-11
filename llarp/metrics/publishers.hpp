#ifndef LLARP_METRICS_PUBLISHERS_HPP
#define LLARP_METRICS_PUBLISHERS_HPP

#include <util/metrics_core.hpp>

#include <util/fs.hpp>

#include <iosfwd>
#include <nlohmann/json.hpp>

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

      ~StreamPublisher()
      {
      }

      void
      publish(const Sample& values) override;
    };

    class JsonPublisher final : public Publisher
    {
     public:
      using PublishFunction = std::function< void(const nlohmann::json&) >;

     private:
      PublishFunction m_publish;

     public:
      JsonPublisher(const PublishFunction& publish) : m_publish(publish)
      {
      }

      ~JsonPublisher()
      {
      }

      void
      publish(const Sample& values) override;

      static void
      directoryPublisher(const nlohmann::json& result, const fs::path& path);
    };
  }  // namespace metrics

}  // namespace llarp

#endif

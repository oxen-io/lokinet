#ifndef LLARP_METRICS_JSON_PUBLISHER_HPP
#define LLARP_METRICS_JSON_PUBLISHER_HPP

#include <util/fs.hpp>
#include <util/metrics/core.hpp>

#include <nlohmann/json.hpp>

#include <functional>
#include <iosfwd>
#include <utility>

namespace llarp
{
  namespace metrics
  {
    class JsonPublisher final : public Publisher
    {
     public:
      using PublishFunction = std::function< void(const nlohmann::json&) >;

     private:
      PublishFunction m_publish;

     public:
      JsonPublisher(PublishFunction publish) : m_publish(std::move(publish))
      {
      }

      ~JsonPublisher() override = default;

      void
      publish(const Sample& values) override;

      static void
      directoryPublisher(const nlohmann::json& result, const fs::path& path);
    };
  }  // namespace metrics
}  // namespace llarp
#endif

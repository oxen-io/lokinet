#ifndef LLARP_METRICS_JSON_PUBLISHER_HPP
#define LLARP_METRICS_JSON_PUBLISHER_HPP

#include <util/fs.hpp>
#include <util/metrics_core.hpp>

#include <nlohmann/json.hpp>

#include <functional>
#include <iosfwd>

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

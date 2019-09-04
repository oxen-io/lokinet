#ifndef LLARP_METRICS_HPP
#define LLARP_METRICS_HPP

#include <util/metrics/types.hpp>
#include <util/metrics/core.hpp>
#include <util/string_view.hpp>

namespace llarp
{
  namespace metrics
  {
    struct MetricsHelper
    {
      static void
      initContainer(CategoryContainer& container, const char* category)
      {
        Manager* manager   = DefaultManager::instance();
        Registry& registry = manager->registry();
        registry.registerContainer(registry.get(category), container);
      }

      static void
      setType(const Id& id, Publication::Type type)
      {
        Manager* manager = DefaultManager::instance();
        return manager->registry().publicationType(id, type);
      }
    };

    template < typename... TagVals >
    void
    integerTick(string_view category, string_view metric, int val,
                TagVals&&... tags)
    {
      if(DefaultManager::instance())
      {
        CollectorRepo< int >& repository =
            DefaultManager::instance()->intCollectorRepo();
        IntCollector* collector = repository.defaultCollector(category, metric);
        if(collector->id().category()->enabled())
        {
          collector->tick(val, tags...);
        }
      }
    }
  }  // namespace metrics
}  // namespace llarp

// Some MSVC flags mess with __LINE__, but __COUNTER__ is better anyway
#ifdef _MSC_VER
#define METRICS_UNIQ_NUMBER __COUNTER__
#else
#define METRICS_UNIQ_NUMBER __LINE__
#endif

// Use a level of indirection to force the preprocessor to expand args first.
#define METRICS_NAME_CAT_IMP(X, Y) X##Y
#define METRICS_NAME_CAT(X, Y) METRICS_NAME_CAT_IMP(X, Y)

#define METRICS_UNIQUE_NAME(X) METRICS_NAME_CAT(X, METRICS_UNIQ_NUMBER)

// For when the category/metric may change during the program run
#define METRICS_DYNAMIC_UPDATE(CAT, METRIC, ...)             \
  do                                                         \
  {                                                          \
    using namespace llarp::metrics;                          \
    if(DefaultManager::instance())                           \
    {                                                        \
      CollectorRepo< double >& repository =                  \
          DefaultManager::instance()->doubleCollectorRepo(); \
      DoubleCollector* collector =                           \
          repository.defaultCollector((CAT), (METRIC));      \
      if(collector->id().category()->enabled())              \
      {                                                      \
        collector->tick(__VA_ARGS__);                        \
      }                                                      \
    }                                                        \
  } while(false)

#endif

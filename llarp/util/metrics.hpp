#ifndef LLARP_METRICS_HPP
#define LLARP_METRICS_HPP

#include <util/metrics_types.hpp>
#include <util/metrics_core.hpp>

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

#define METRICS_TIME_BLOCK_IMP(CAT, METRIC, VAR_NAME)      \
  llarp::metrics::DoubleCollector* VAR_NAME = nullptr;     \
  if(llarp::metrics::DefaultManager::instance())           \
  {                                                        \
    using namespace llarp::metrics;                        \
    CollectorRepo< double >& repo =                        \
        DefaultManager::instance()->doubleCollectorRepo(); \
    VAR_NAME = repo.defaultCollector((CAT), (METRIC));     \
  }                                                        \
  llarp::metrics::TimerGuard METRICS_UNIQUE_NAME(timer_guard)(VAR_NAME);

#define METRICS_TIME_BLOCK(CAT, METRIC) \
  METRICS_TIME_BLOCK_IMP(CAT, METRIC, METRICS_UNIQUE_NAME(time_block))

#define METRICS_IF_CATEGORY_ENABLED_IMP(CAT, NAME)                           \
  static llarp::metrics::CategoryContainer NAME = {false, nullptr, nullptr}; \
  if(!NAME.category() && llarp::metrics::DefaultManager::instance())         \
  {                                                                          \
    llarp::metrics::MetricsHelper::initContainer(NAME, CAT);                 \
  }                                                                          \
  if(NAME.enabled())

#define METRICS_IF_CATEGORY_ENABLED(CAT) \
  BALM_METRICS_IF_CATEGORY_ENABLED_IMP(CAT, METRICS_UNIQUE_NAME(Container))

// For when the category/metric may change during the program run
#define METRICS_DYNAMIC_INT_UPDATE(CAT, METRIC, VALUE)                        \
  do                                                                          \
  {                                                                           \
    using namespace llarp::metrics;                                           \
    if(DefaultManager::instance())                                            \
    {                                                                         \
      CollectorRepo< int >& repository =                                      \
          DefaultManager::instance()->intCollectorRepo();                     \
      IntCollector* collector = repository.defaultCollector((CAT), (METRIC)); \
      if(collector->id().category()->enabled())                               \
      {                                                                       \
        collector->tick((VALUE));                                             \
      }                                                                       \
    }                                                                         \
  } while(false)

// For when the category/metric remain static
#define METRICS_INT_UPDATE(CAT, METRIC, VALUE)                        \
  do                                                                  \
  {                                                                   \
    using namespace llarp::metrics;                                   \
    static CategoryContainer container = {false, nullptr, nullptr};   \
    static IntCollector* collector     = nullptr;                     \
    if(container.category() == nullptr && DefaultManager::instance()) \
    {                                                                 \
      collector = MetricHelper::getIntCollector(CAT, METRIC);         \
      MetricHelper::initContainer(container, CAT);                    \
    }                                                                 \
    if(container.enabled())                                           \
    {                                                                 \
      collector->tick(VALUE);                                         \
    }                                                                 \
  } while(false)

#define METRICS_TYPED_INT_UPDATE(CAT, METRIC, VALUE, TYPE)            \
  do                                                                  \
  {                                                                   \
    using namespace llarp::metrics;                                   \
    static CategoryContainer container = {false, nullptr, nullptr};   \
    static IntCollector* collector     = nullptr;                     \
    if(container.category() == nullptr && DefaultManager::instance()) \
    {                                                                 \
      collector = MetricHelper::getIntCollector(CAT, METRIC);         \
      MetricHelper::setType(collector->id(), TYPE);                   \
      MetricHelper::initContainer(container, CAT);                    \
    }                                                                 \
    if(container.enabled())                                           \
    {                                                                 \
      collector->tick(VALUE);                                         \
    }                                                                 \
  } while(false)

// For when the category/metric may change during the program run
#define METRICS_DYNAMIC_UPDATE(CAT, METRIC, VALUE)           \
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
        collector->tick((VALUE));                            \
      }                                                      \
    }                                                        \
  } while(false)

// For when the category/metric remain static
#define METRICS_UPDATE(CAT, METRIC, VALUE)                            \
  do                                                                  \
  {                                                                   \
    using namespace llarp::metrics;                                   \
    static CategoryContainer container = {false, nullptr, nullptr};   \
    static DoubleCollector* collector  = nullptr;                     \
    if(container.category() == nullptr && DefaultManager::instance()) \
    {                                                                 \
      collector = MetricHelper::getDoubleCollector(CAT, METRIC);      \
      MetricHelper::initContainer(container, CAT);                    \
    }                                                                 \
    if(container.enabled())                                           \
    {                                                                 \
      collector->tick(VALUE);                                         \
    }                                                                 \
  } while(false)

#define METRICS_TYPED_UPDATE(CAT, METRIC, VALUE, TYPE)                \
  do                                                                  \
  {                                                                   \
    using namespace llarp::metrics;                                   \
    static CategoryContainer container = {false, nullptr, nullptr};   \
    static DoubleCollector* collector  = nullptr;                     \
    if(container.category() == nullptr && DefaultManager::instance()) \
    {                                                                 \
      collector = MetricHelper::getDoubleCollector(CAT, METRIC);      \
      MetricHelper::setType(collector->id(), TYPE);                   \
      MetricHelper::initContainer(container, CAT);                    \
    }                                                                 \
    if(container.enabled())                                           \
    {                                                                 \
      collector->tick(VALUE);                                         \
    }                                                                 \
  } while(false)

#define METRICS_DYNAMIC_INCREMENT(CAT, METRIC) \
  METRICS_DYNAMIC_INT_UPDATE(CAT, METRIC, 1)

#define METRICS_INCREMENT(CAT, METRIC) METRICS_INT_UPDATE(CAT, METRIC, 1)

#endif

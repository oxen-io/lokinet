#ifndef LLARP_METRICS_CORE_HPP
#define LLARP_METRICS_CORE_HPP

#include <util/metrics_types.hpp>
#include <util/scheduler.hpp>
#include <util/stopwatch.hpp>
#include <util/threading.hpp>

#include <map>
#include <memory>
#include <vector>

namespace llarp
{
  namespace metrics
  {
    class IntCollector
    {
      const Id m_id;
      size_t m_count GUARDED_BY(m_mutex);
      int64_t m_total GUARDED_BY(m_mutex);
      int m_min GUARDED_BY(m_mutex);
      int m_max GUARDED_BY(m_mutex);
      mutable util::Mutex m_mutex;

      IntCollector(const IntCollector &) = delete;
      IntCollector &
      operator=(const IntCollector &) = delete;

     public:
      static constexpr int DEFAULT_MIN = std::numeric_limits< int >::max();
      static constexpr int DEFAULT_MAX = std::numeric_limits< int >::min();

      IntCollector(const Id &id)
          : m_id(id)
          , m_count(0)
          , m_total(0)
          , m_min(DEFAULT_MIN)
          , m_max(DEFAULT_MAX)
      {
      }

      const Id &
      id() const
      {
        return m_id;
      }

      void
      clear()
      {
        absl::WriterMutexLock l(&m_mutex);
        m_count = 0;
        m_total = 0;
        m_min   = DEFAULT_MIN;
        m_max   = DEFAULT_MAX;
      }

      Record
      loadAndClear();

      Record
      load();

      void
      tick(int value)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_count++;
        m_total += value;
        m_min = std::min(m_min, value);
        m_max = std::max(m_max, value);
      }

      void
      accumulate(size_t count, int total, int min, int max)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_count += count;
        m_total += total;
        m_min = std::min(m_min, min);
        m_max = std::max(m_max, max);
      }

      void
      set(size_t count, int total, int min, int max)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_count = count;
        m_total = total;
        m_min   = min;
        m_max   = max;
      }
    };

    class DoubleCollector
    {
      Record m_record GUARDED_BY(m_mutex);
      mutable util::Mutex m_mutex;

      DoubleCollector(const DoubleCollector &) = delete;
      DoubleCollector &
      operator=(const DoubleCollector &) = delete;

     public:
      DoubleCollector(const Id &id) : m_record(id)
      {
      }

      void
      clear()
      {
        absl::WriterMutexLock l(&m_mutex);
        m_record.count() = 0;
        m_record.total() = 0.0;
        m_record.min()   = Record::DEFAULT_MIN;
        m_record.max()   = Record::DEFAULT_MAX;
      }

      Record
      loadAndClear()
      {
        absl::WriterMutexLock l(&m_mutex);
        Record rec       = m_record;
        m_record.count() = 0;
        m_record.total() = 0.0;
        m_record.min()   = Record::DEFAULT_MIN;
        m_record.max()   = Record::DEFAULT_MAX;

        return rec;
      }

      Record
      load()
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_record;
      }

      void
      tick(double value)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_record.count()++;
        m_record.total() += value;
        m_record.min() = std::min(m_record.min(), value);
        m_record.max() = std::max(m_record.max(), value);
      }

      void
      accumulate(size_t count, double total, double min, double max)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_record.count() += count;
        m_record.total() += total;
        m_record.min() = std::min(m_record.min(), min);
        m_record.max() = std::max(m_record.max(), max);
      }

      void
      set(size_t count, double total, double min, double max)
      {
        absl::WriterMutexLock l(&m_mutex);
        m_record.count() = count;
        m_record.total() = total;
        m_record.min()   = min;
        m_record.max()   = max;
      }

      const Id &
      id() const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_record.id();
      }
    };

    class Publisher
    {
     public:
      virtual ~Publisher() = 0;

      virtual void
      publish(const Sample &sample) = 0;
    };

    inline Publisher::~Publisher()
    {
    }

    static inline void
    combine(Record &record, const Record &toAdd)
    {
      record.id() = toAdd.id();
      record.count() += toAdd.count();
      record.total() += toAdd.total();
      record.min() = std::min(record.min(), toAdd.min());
      record.max() = std::max(record.max(), toAdd.max());
    }

    template < typename Collector >
    class Collectors
    {
      using CollectorPtr = std::shared_ptr< Collector >;
      using CollectorSet = std::set< CollectorPtr >;

      Collector m_default;
      CollectorSet m_collectors;

      Collectors(const Collectors &) = delete;
      Collectors &
      operator=(const Collectors &) = delete;

     public:
      Collectors(const Id &id) : m_default(id)
      {
      }

      Collector *
      defaultCollector()
      {
        return &m_default;
      }

      std::shared_ptr< Collector >
      add()
      {
        auto ptr = std::make_shared< Collector >(m_default.id());
        m_collectors.insert(ptr);
        return ptr;
      }

      bool
      remove(Collector *collector)
      {
        std::shared_ptr< Collector > ptr(collector, [](Collector *) {});
        size_t count = m_collectors.erase(ptr);
        return count > 0;
      }

      Record
      combineAndClear()
      {
        Record rec = m_default.loadAndClear();

        for(auto &ptr : m_collectors)
        {
          metrics::combine(rec, ptr->loadAndClear());
        }

        return rec;
      }

      Record
      combine()
      {
        Record rec = m_default.load();

        for(auto &ptr : m_collectors)
        {
          metrics::combine(rec, ptr->load());
        }
        return rec;
      }

      std::vector< std::shared_ptr< Collector > >
      collectors() const
      {
        return std::vector< std::shared_ptr< Collector > >(m_collectors.begin(),
                                                           m_collectors.end());
      }

      const Id &
      id() const
      {
        return m_default.id();
      }
    };

    class MetricCollectors
    {
      using DoubleCollectors = Collectors< DoubleCollector >;
      using IntCollectors    = Collectors< IntCollector >;

      DoubleCollectors m_doubleCollectors;
      IntCollectors m_intCollectors;

      MetricCollectors(const MetricCollectors &) = delete;
      MetricCollectors &
      operator=(const MetricCollectors &) = delete;

     public:
      MetricCollectors(const Id &id)
          : m_doubleCollectors(id), m_intCollectors(id)
      {
      }

      Collectors< DoubleCollector > &
      doubleCollectors()
      {
        return m_doubleCollectors;
      }

      Collectors< IntCollector > &
      intCollectors()
      {
        return m_intCollectors;
      }

      const Collectors< DoubleCollector > &
      doubleCollectors() const
      {
        return m_doubleCollectors;
      }

      const Collectors< IntCollector > &
      intCollectors() const
      {
        return m_intCollectors;
      }

      Record
      combineAndClear()
      {
        Record res = m_doubleCollectors.combineAndClear();
        metrics::combine(res, m_intCollectors.combineAndClear());
        return res;
      }

      Record
      combine()
      {
        Record res = m_doubleCollectors.combine();
        metrics::combine(res, m_intCollectors.combine());
        return res;
      }

      const Id &
      id() const
      {
        return m_intCollectors.id();
      }
    };

    class Registry
    {
      using NamedCategory = std::tuple< const char *, const char * >;

      struct CmpNamedCategory
      {
        bool
        operator()(const NamedCategory &lhs, const NamedCategory &rhs) const
        {
          int ret = std::strcmp(std::get< 0 >(lhs), std::get< 0 >(rhs));
          if(ret == 0)
          {
            ret = std::strcmp(std::get< 1 >(lhs), std::get< 1 >(rhs));
          }
          return ret < 0;
        }
      };
      struct StrCmp
      {
        bool
        operator()(const char *lhs, const char *rhs) const
        {
          return std::strcmp(lhs, rhs) < 0;
        }
      };

      using MetricMap = std::map< NamedCategory, std::shared_ptr< Description >,
                                  CmpNamedCategory >;
      using CategoryMap =
          std::map< const char *, std::shared_ptr< Category >, StrCmp >;

      std::set< std::string > m_stringmem GUARDED_BY(m_mutex);
      CategoryMap m_categories GUARDED_BY(m_mutex);
      MetricMap m_metrics GUARDED_BY(m_mutex);
      bool m_defaultEnabled GUARDED_BY(m_mutex);
      mutable util::Mutex m_mutex;

      Registry(const Registry &) = delete;
      Registry &
      operator=(const Registry &) = delete;

      std::tuple< Id, bool >
      insert(const char *category, const char *name)
          EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

     public:
      Registry() : m_defaultEnabled(true)
      {
      }

      Id
      add(const char *category, const char *name) LOCKS_EXCLUDED(m_mutex);
      Id
      get(const char *category, const char *name) LOCKS_EXCLUDED(m_mutex);

      const Category *
      add(const char *category) LOCKS_EXCLUDED(m_mutex);
      const Category *
      get(const char *category);

      void
      enable(const Category *category, bool value);
      void
      enableAll(bool value);

      void
      registerContainer(const Category *category, CategoryContainer &container);
      void
      publicationType(const Id &id, Publication::Type type);
      void
      setFormat(const Id &id, const Format &format);

      size_t
      metricCount() const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_metrics.size();
      }
      size_t
      categoryCount() const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_categories.size();
      }

      const Category *
      findCategory(const char *category) const;
      Id
      findId(const char *category, const char *name) const;

      std::vector< const Category * >
      getAll() const;
    };

    class CollectorRepo
    {
      using MetricCollectorsPtr = std::shared_ptr< MetricCollectors >;
      using IdCollectors        = std::map< Id, MetricCollectorsPtr >;
      using CategoryCollectors =
          std::map< const Category *, std::vector< MetricCollectors * > >;

      Registry *m_registry;
      IdCollectors m_collectors;
      CategoryCollectors m_categories;
      mutable util::Mutex m_mutex;

      CollectorRepo(const CollectorRepo &) = delete;
      CollectorRepo &
      operator=(const CollectorRepo &) = delete;

      MetricCollectors &
      getCollectors(const Id &id);

     public:
      CollectorRepo(Registry *registry) : m_registry(registry)
      {
      }

      std::vector< Record >
      collectAndClear(const Category *category);

      std::vector< Record >
      collect(const Category *category);

      DoubleCollector *
      defaultDoubleCollector(const char *category, const char *name)
      {
        return defaultDoubleCollector(m_registry->get(category, name));
      }

      DoubleCollector *
      defaultDoubleCollector(const Id &id);

      IntCollector *
      defaultIntCollector(const char *category, const char *name)
      {
        return defaultIntCollector(m_registry->get(category, name));
      }

      IntCollector *
      defaultIntCollector(const Id &id);

      std::shared_ptr< DoubleCollector >
      addDoubleCollector(const char *category, const char *name)
      {
        return addDoubleCollector(m_registry->get(category, name));
      }

      std::shared_ptr< DoubleCollector >
      addDoubleCollector(const Id &id)
      {
        absl::WriterMutexLock l(&m_mutex);
        return getCollectors(id).doubleCollectors().add();
      }

      std::shared_ptr< IntCollector >
      addIntCollector(const char *category, const char *name)
      {
        return addIntCollector(m_registry->get(category, name));
      }

      std::shared_ptr< IntCollector >
      addIntCollector(const Id &id)
      {
        absl::WriterMutexLock l(&m_mutex);
        return getCollectors(id).intCollectors().add();
      }

      std::pair< std::vector< std::shared_ptr< DoubleCollector > >,
                 std::vector< std::shared_ptr< IntCollector > > >
      allCollectors(const Id &id);

      Registry &
      registry()
      {
        return *m_registry;
      }

      const Registry &
      registry() const
      {
        return *m_registry;
      }
    };

    class Manager;

    class PublisherRegistry
    {
      using PubPtr        = std::shared_ptr< Publisher >;
      using CatPublishers = std::multimap< const Category *, PubPtr >;
      using PubSet        = std::set< PubPtr >;
      using PubReg  = std::map< const Category *, CatPublishers::iterator >;
      using RegInfo = std::map< PubPtr, PubReg >;

      CatPublishers m_categoryPublishers;
      RegInfo m_registry;
      PubSet m_publishers;

      PublisherRegistry(const PublisherRegistry &) = delete;
      PublisherRegistry &
      operator=(const PublisherRegistry &) = delete;

     public:
      using GlobalIterator = PubSet::iterator;
      using CatIterator    = CatPublishers::iterator;

      PublisherRegistry() = default;

      bool
      addGlobalPublisher(const std::shared_ptr< Publisher > &publisher)
      {
        if(m_publishers.find(publisher) != m_publishers.end())
        {
          return false;
        }
        if(m_registry.find(publisher) != m_registry.end())
        {
          return false;
        }

        m_publishers.insert(publisher);
        return true;
      }

      bool
      addPublisher(const Category *category,
                   const std::shared_ptr< Publisher > &publisher)
      {
        if(m_publishers.find(publisher) != m_publishers.end())
        {
          return false;
        }

        auto &reg = m_registry[publisher];
        if(reg.find(category) != reg.end())
        {
          return false;
        }

        auto it = m_categoryPublishers.emplace(category, publisher);
        reg.emplace(category, it);
        return true;
      }

      bool
      removePublisher(const Publisher *publisher)
      {
        std::shared_ptr< Publisher > ptr(const_cast< Publisher * >(publisher),
                                         [](Publisher *) {});

        auto allIt = m_publishers.find(ptr);

        if(allIt != m_publishers.end())
        {
          m_publishers.erase(allIt);
          return true;
        }

        auto regIt = m_registry.find(ptr);
        if(regIt == m_registry.end())
        {
          return false;
        }

        for(auto &spec : regIt->second)
        {
          m_categoryPublishers.erase(spec.second);
        }

        m_registry.erase(regIt);
        return true;
      }

      GlobalIterator
      globalBegin()
      {
        return m_publishers.begin();
      }
      GlobalIterator
      globalEnd()
      {
        return m_publishers.end();
      }

      CatIterator
      catBegin()
      {
        return m_categoryPublishers.begin();
      }
      CatIterator
      catEnd()
      {
        return m_categoryPublishers.end();
      }

      CatIterator
      lowerBound(const Category *category)
      {
        return m_categoryPublishers.lower_bound(category);
      }
      CatIterator
      upperBound(const Category *category)
      {
        return m_categoryPublishers.upper_bound(category);
      }

      std::vector< Publisher * >
      globalPublishers() const
      {
        std::vector< Publisher * > result;
        result.reserve(m_publishers.size());

        std::transform(m_publishers.begin(), m_publishers.end(),
                       std::back_inserter(result),
                       [](const auto &p) { return p.get(); });

        return result;
      }

      std::vector< Publisher * >
      catPublishers(const Category *category) const
      {
        std::vector< Publisher * > result;
        auto beg = m_categoryPublishers.lower_bound(category);
        auto end = m_categoryPublishers.upper_bound(category);
        result.reserve(std::distance(beg, end));

        std::transform(beg, end, std::back_inserter(result),
                       [](const auto &p) { return p.second.get(); });

        return result;
      }
    };

    class CallbackRegistry
    {
      using Handle         = uint64_t;
      using RecordCallback = std::function< std::vector< Record >(bool) >;
      using CallbackMap    = std::multimap< const Category *, RecordCallback >;
      using HandleMap      = std::map< Handle, CallbackMap::iterator >;

      Handle m_next;
      CallbackMap m_callbackMap;
      HandleMap m_handleMap;

      CallbackRegistry(const CallbackRegistry &) = delete;
      CallbackRegistry &
      operator=(const CallbackRegistry &) = delete;

     public:
      using iterator = CallbackMap::iterator;

      CallbackRegistry() : m_next(1)
      {
      }

      Handle
      registerCallback(const Category *category, const RecordCallback &callback)
      {
        Handle handle = m_next++;

        auto it = m_callbackMap.emplace(category, callback);
        m_handleMap.emplace(handle, it);

        return handle;
      }

      bool
      removeCallback(Handle handle)
      {
        auto it = m_handleMap.find(handle);

        if(it == m_handleMap.end())
        {
          return false;
        }

        m_callbackMap.erase(it->second);
        m_handleMap.erase(it);
        return true;
      }

      iterator
      begin()
      {
        return m_callbackMap.begin();
      }
      iterator
      end()
      {
        return m_callbackMap.end();
      }

      iterator
      lowerBound(const Category *category)
      {
        return m_callbackMap.lower_bound(category);
      }
      iterator
      upperBound(const Category *category)
      {
        return m_callbackMap.upper_bound(category);
      }

      std::vector< const RecordCallback * >
      callbacksFor(const Category *category) const
      {
        std::vector< const RecordCallback * > result;
        auto beg = m_callbackMap.lower_bound(category);
        auto end = m_callbackMap.upper_bound(category);

        result.reserve(std::distance(beg, end));
        std::transform(beg, end, std::back_inserter(result),
                       [](const auto &x) { return &x.second; });

        return result;
      }
    };

    struct PublisherHelper;

    /// The big dog.
    /// This class owns everything else, and is responsible for managing the
    /// gathering and publishing of metrics
    class Manager
    {
     public:
      // Public callback. If the bool flag is true, clear the metrics back to
      // their default state.
      using RecordCallback = std::function< std::vector< Record >(bool) >;

      using Handle = uint64_t;

     private:
      // Map categories to the times they were last reset
      using ResetTimes = std::map< const Category *, absl::Duration >;

      friend struct PublisherHelper;

      Registry m_registry;
      CollectorRepo m_repo;
      CallbackRegistry m_callbacks GUARDED_BY(m_mutex);
      PublisherRegistry m_publishers GUARDED_BY(m_mutex);

      const absl::Duration m_createTime;
      ResetTimes m_resetTimes;

      util::Mutex m_publishLock ACQUIRED_BEFORE(m_mutex);
      mutable util::Mutex m_mutex ACQUIRED_AFTER(m_publishLock);

     public:
      static constexpr Handle INVALID_HANDLE =
          std::numeric_limits< Handle >::max();

      Manager()
          : m_repo(&m_registry), m_createTime(absl::Now() - absl::UnixEpoch())
      {
      }

      /// Register a callback for
      Handle
      registerCallback(const char *categoryName, const RecordCallback &callback)
      {
        return registerCallback(m_registry.get(categoryName), callback);
      }
      Handle
      registerCallback(const Category *category, const RecordCallback &callback)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_callbacks.registerCallback(category, callback);
      }

      bool
      removeCallback(Handle handle)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_callbacks.removeCallback(handle);
      }

      /// Add a `publisher` which will receive all events
      bool
      addGlobalPublisher(const std::shared_ptr< Publisher > &publisher)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_publishers.addGlobalPublisher(publisher);
      }

      /// Add a `publisher` which will receive events for the given
      /// `categoryName` only
      bool
      addPublisher(const char *categoryName,
                   const std::shared_ptr< Publisher > &publisher)
      {
        return addPublisher(m_registry.get(categoryName), publisher);
      }
      /// Add a `publisher` which will receive events for the given
      /// `category` only
      bool
      addPublisher(const Category *category,
                   const std::shared_ptr< Publisher > &publisher)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_publishers.addPublisher(category, publisher);
      }

      bool
      removePublisher(const Publisher *publisher)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_publishers.removePublisher(publisher);
      }
      bool
      removePublisher(const std::shared_ptr< Publisher > &publisher)
      {
        absl::WriterMutexLock l(&m_mutex);
        return m_publishers.removePublisher(publisher.get());
      }

      // clang-format off
      CollectorRepo&       collectorRepo()       { return m_repo; }
      const CollectorRepo& collectorRepo() const { return m_repo; }
      Registry&            registry()            { return m_registry; }
      const Registry&      registry() const      { return m_registry; }
      // clang-format on

      /// Publish specific categories of metric matching the category/categories
      Sample
      collectSample(std::vector< Record > &records, bool clear = false)
      {
        std::vector< const Category * > allCategories = m_registry.getAll();
        return collectSample(
            records, absl::Span< const Category * >{allCategories}, clear);
      }
      Sample
      collectSample(std::vector< Record > &records,
                    absl::Span< const Category * > categories,
                    bool clear = false);

      /// Publish specific categories of metric matching the category/categories
      void
      publish(const Category *category, bool clear = true)
      {
        publish(absl::Span< const Category * >(&category, 1), clear);
      }
      void
      publish(absl::Span< const Category * > categories, bool clear = true);
      void
      publish(const std::set< const Category * > &categories,
              bool clear = true);

      void
      publishAll(bool clear = true)
      {
        std::vector< const Category * > allCategories = m_registry.getAll();
        publish(absl::Span< const Category * >{allCategories}, clear);
      }

      void
      publishAllExcluding(const std::set< const Category * > &excludeCategories,
                          bool clear = true)
      {
        if(excludeCategories.empty())
        {
          publishAll(clear);
          return;
        }

        std::vector< const Category * > allCategories = m_registry.getAll();
        std::vector< const Category * > includedCategories;
        includedCategories.reserve(allCategories.size()
                                   - excludeCategories.size());

        std::copy_if(
            allCategories.begin(), allCategories.end(),
            std::back_inserter(includedCategories), [&](const Category *cat) {
              return excludeCategories.end() == excludeCategories.find(cat);
            });

        if(!includedCategories.empty())
        {
          publish(absl::Span< const Category * >{includedCategories}, clear);
        }
      }

      void
      enableCategory(const char *categoryName, bool enable = true)
      {
        m_registry.enable(m_registry.get(categoryName), enable);
      }
      void
      enableCategory(const Category *category, bool enable = true)
      {
        m_registry.enable(category, enable);
      }

      void
      enableAll(bool enable)
      {
        m_registry.enableAll(enable);
      }

      std::vector< Publisher * >
      globalPublishers() const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_publishers.globalPublishers();
      }

      std::vector< Publisher * >
      publishersForCategory(const char *categoryName) const
      {
        const Category *category = m_registry.findCategory(categoryName);
        return category ? publishersForCategory(category)
                        : std::vector< Publisher * >();
      }
      std::vector< Publisher * >
      publishersForCategory(const Category *category) const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_publishers.catPublishers(category);
      }
    };

    /// Provide a handy mechanism for retrieving the default manager, without
    /// a painful singleton mechanism
    class DefaultManager
    {
      static Manager *m_manager;

     public:
      static Manager *
      instance()
      {
        return m_manager;
      }

      static Manager *
      manager(Manager *value)
      {
        return value ? value : m_manager;
      }

      static Manager *
      create()
      {
        m_manager = new Manager;
        return m_manager;
      }

      static void
      destroy()
      {
        delete m_manager;
        m_manager = nullptr;
      }
    };

    /// Scoped guard to manage the default manager
    class DefaultManagerGuard
    {
      DefaultManagerGuard(const DefaultManagerGuard &) = delete;
      DefaultManagerGuard &
      operator=(const DefaultManagerGuard &) = delete;

     public:
      DefaultManagerGuard()
      {
        DefaultManager::create();
      }

      ~DefaultManagerGuard()
      {
        DefaultManager::destroy();
      }

      Manager *
      instance()
      {
        return DefaultManager::instance();
      }
    };

    template < typename Collector, typename Value,
               Collector *(CollectorRepo::*catFunc)(const char *, const char *),
               Collector *(CollectorRepo::*idFunc)(const Id &) >
    class Metric
    {
      Collector *m_collector;  // can be null
      const std::atomic_bool *m_enabled;

     public:
      static Collector *
      lookup(const char *category, const char *name, Manager *manager = nullptr)
      {
        manager = DefaultManager::manager(manager);
        return manager ? (manager->collectorRepo().*catFunc)(category, name)
                       : 0;
      }

      static Collector *
      lookup(const Id &id, Manager *manager = nullptr)
      {
        manager = DefaultManager::manager(manager);
        return manager ? (manager->collectorRepo().*idFunc)(id) : 0;
      }

      Metric(const char *category, const char *name, Manager *manager)
          : m_collector(lookup(category, name, manager))
          , m_enabled(m_collector ? &m_collector->id().category()->enabledRaw()
                                  : nullptr)
      {
      }

      Metric(const Id &id, Manager *manager)
          : m_collector(lookup(id, manager))
          , m_enabled(m_collector ? &m_collector->id().category()->enabledRaw()
                                  : nullptr)
      {
      }

      Metric(Collector *collector)
          : m_collector(collector)
          , m_enabled(m_collector ? &m_collector->id().category()->enabledRaw()
                                  : nullptr)
      {
      }

      bool
      active() const
      {
        return m_enabled ? m_enabled->load(std::memory_order_relaxed) : false;
      }

      void
      tick()
      {
        if(active())
        {
          m_collector->tick(static_cast< Value >(1));
        }
      }

      void
      update(Value val)
      {
        if(active())
        {
          m_collector->tick(val);
        }
      }

      void
      accumulate(size_t count, Value total, Value min, Value max)
      {
        if(active())
        {
          m_collector->accumulate(count, total, min, max);
        }
      }

      void
      set(size_t count, Value total, Value min, Value max)
      {
        if(active())
        {
          m_collector->set(count, total, min, max);
        }
      }

      Id
      id() const
      {
        return m_collector ? m_collector->id() : Id();
      }

      const Collector *
      collector() const
      {
        return m_collector;
      }

      Collector *
      collector()
      {
        return m_collector;
      }

      static void
      getCollector(Collector **collector, CategoryContainer *container,
                   const char *category, const char *metric)
      {
        Manager *manager = DefaultManager::instance();
        *collector       = manager->collectorRepo().*catFunc(category, metric);
        manager->registry().registerContainer((*collector)->id().category(),
                                              container);
      }

      static void
      getCollector(Collector **collector, CategoryContainer *container,
                   const char *category, const char *metric,
                   Publication::Type type)
      {
        Manager *manager = DefaultManager::instance();
        *collector       = manager->collectorRepo().*catFunc(category, metric);
        manager->registry().registerContainer((*collector)->id().category(),
                                              container);
        manager->registry().publicationType((*collector)->id(), type);
      }
    };

    using DoubleMetric =
        Metric< DoubleCollector, double, &CollectorRepo::defaultDoubleCollector,
                &CollectorRepo::defaultDoubleCollector >;

    using IntMetric =
        Metric< IntCollector, int, &CollectorRepo::defaultIntCollector,
                &CollectorRepo::defaultIntCollector >;

    class TimerGuard
    {
     private:
      util::Stopwatch m_stopwatch;
      DoubleCollector *m_collector;

      TimerGuard(const TimerGuard &) = delete;
      TimerGuard &
      operator=(const TimerGuard &) = delete;

     public:
      TimerGuard(DoubleMetric *metric)
          : m_stopwatch()
          , m_collector(metric->active() ? metric->collector() : nullptr)
      {
        if(m_collector)
        {
          m_stopwatch.start();
        }
      }

      TimerGuard(DoubleCollector *collector)
          : m_stopwatch()
          , m_collector(collector && collector->id().category()->enabled()
                            ? collector
                            : nullptr)
      {
        if(m_collector)
        {
          m_stopwatch.start();
        }
      }

      TimerGuard(const char *category, const char *name, Manager *manager)
          : m_stopwatch(), m_collector(nullptr)
      {
        DoubleCollector *collector =
            DoubleMetric::lookup(category, name, manager);
        m_collector = (collector && collector->id().category()->enabled())
            ? collector
            : nullptr;
        if(m_collector)
        {
          m_stopwatch.start();
        }
      }
      TimerGuard(const Id &id, Manager *manager)
          : m_stopwatch(), m_collector(nullptr)
      {
        DoubleCollector *collector = DoubleMetric::lookup(id, manager);
        m_collector = (collector && collector->id().category()->enabled())
            ? collector
            : nullptr;
        if(m_collector)
        {
          m_stopwatch.start();
        }
      }

      ~TimerGuard()
      {
        if(active())
        {
          m_stopwatch.stop();
          m_collector->tick(absl::ToDoubleSeconds(m_stopwatch.time()));
        }
      }

      bool
      active() const
      {
        return m_collector ? m_collector->id().category()->enabled() : false;
      }
    };

    struct PublisherSchedulerData;

    class PublisherScheduler
    {
      friend class PublisherSchedulerGuard;

      using Categories = std::map< const Category *, absl::Duration >;
      using Repeaters =
          std::map< absl::Duration, std::shared_ptr< PublisherSchedulerData > >;

      thread::Scheduler &m_scheduler;
      Manager *m_manager;

      Categories m_categories GUARDED_BY(m_mutex);
      Repeaters m_repeaters GUARDED_BY(m_mutex);
      absl::Duration m_defaultInterval GUARDED_BY(m_mutex);

      mutable util::Mutex m_mutex;

      void
      publish(const std::shared_ptr< PublisherSchedulerData > &data) const;

      void
      cancel(Categories::iterator it) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

      bool
      cancelDefault() EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

     public:
      PublisherScheduler(thread::Scheduler &scheduler, Manager *manager)
          : m_scheduler(scheduler), m_manager(manager), m_defaultInterval()
      {
      }

      ~PublisherScheduler()
      {
        cancelAll();
      }

      void
      schedule(const char *categoryName, absl::Duration interval)
      {
        return schedule(m_manager->registry().get(categoryName), interval);
      }

      void
      schedule(const Category *category, absl::Duration interval);

      void
      setDefault(absl::Duration interval);

      bool
      cancel(const char *categoryName)
      {
        return cancel(m_manager->registry().get(categoryName));
      }

      bool
      cancel(const Category *category);

      bool
      clearDefault();

      void
      cancelAll();

      Manager *
      manager()
      {
        return m_manager;
      }
      const Manager *
      manager() const
      {
        return m_manager;
      }

      absl::optional< absl::Duration >
      find(const char *categoryName) const
      {
        return find(m_manager->registry().get(categoryName));
      }

      absl::optional< absl::Duration >
      find(const Category *category) const;

      absl::optional< absl::Duration >
      getDefault() const;

      std::vector< std::pair< const Category *, absl::Duration > >
      getAll() const;
    };

  }  // namespace metrics
}  // namespace llarp

#endif

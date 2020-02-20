#include <util/metrics/core.hpp>

#include <iostream>

namespace llarp
{
  namespace metrics
  {
    std::pair< Id, bool >
    Registry::insert(string_view category, string_view name)
    {
      // avoid life time issues, putting strings in the stringmem set
      string_view cStr = m_stringmem.emplace(category).first->c_str();
      string_view nStr = m_stringmem.emplace(name).first->c_str();

      NamedCategory namedCategory(cStr, nStr);
      const auto it = m_metrics.find(namedCategory);

      if(it != m_metrics.end())
      {
        return {Id(it->second.get()), false};
      }

      auto cIt = m_categories.find(cStr);
      if(cIt == m_categories.end())
      {
        auto ptr = std::make_shared< Category >(cStr, m_defaultEnabled);
        cIt      = m_categories.emplace(cStr, ptr).first;
      }

      const auto mPtr =
          std::make_shared< Description >(cIt->second.get(), nStr);

      m_metrics.emplace(namedCategory, mPtr);
      return {Id(mPtr.get()), true};
    }

    Id
    Registry::add(string_view category, string_view name)
    {
      absl::WriterMutexLock l(&m_mutex);
      auto result = insert(category, name);
      return std::get< 1 >(result) ? std::get< 0 >(result) : Id();
    }

    Id
    Registry::get(string_view category, string_view name)
    {
      Id result = findId(category, name);
      if(result)
      {
        return result;
      }

      absl::WriterMutexLock l(&m_mutex);
      return std::get< 0 >(insert(category, name));
    }

    const Category *
    Registry::add(string_view category)
    {
      absl::WriterMutexLock l(&m_mutex);

      string_view cStr = m_stringmem.emplace(category).first->c_str();
      auto it          = m_categories.find(cStr);
      if(it == m_categories.end())
      {
        auto ptr = std::make_shared< Category >(cStr, m_defaultEnabled);
        it       = m_categories.emplace(cStr, ptr).first;
        return it->second.get();
      }
      return nullptr;
    }

    const Category *
    Registry::get(string_view category)
    {
      const Category *cPtr = findCategory(category);
      if(cPtr)
      {
        return cPtr;
      }

      absl::WriterMutexLock l(&m_mutex);
      string_view cStr = m_stringmem.emplace(category).first->c_str();
      auto it          = m_categories.find(cStr);
      if(it == m_categories.end())
      {
        auto ptr = std::make_shared< Category >(cStr, m_defaultEnabled);
        it       = m_categories.emplace(cStr, ptr).first;
      }
      return it->second.get();
    }

    void
    Registry::enable(const Category *category, bool value)
    {
      absl::WriterMutexLock l(&m_mutex);
      const_cast< Category * >(category)->enabled(value);
    }

    void
    Registry::enableAll(bool value)
    {
      absl::WriterMutexLock l(&m_mutex);

      if(value == m_defaultEnabled)
      {
        return;
      }

      m_defaultEnabled = value;

      std::for_each(m_categories.begin(), m_categories.end(),
                    [&](auto &x) { x.second->enabled(value); });
    }

    void
    Registry::registerContainer(const Category *category,
                                CategoryContainer &container)
    {
      absl::WriterMutexLock l(&m_mutex);
      if(container.m_category == nullptr)
      {
        const_cast< Category * >(category)->registerContainer(&container);
      }
    }

    void
    Registry::publicationType(const Id &id, Publication::Type type)
    {
      const_cast< Description * >(id.description())->type(type);
    }

    void
    Registry::setFormat(const Id &id, const Format &format)
    {
      auto *description = const_cast< Description * >(id.description());

      absl::WriterMutexLock l(&m_mutex);

      auto fmtPtr = std::make_shared< Format >(format);

      for(byte_t i = 0; i < Publication::MaxSize; ++i)
      {
        auto type              = static_cast< Publication::Type >(i);
        const FormatSpec *spec = format.specFor(type);
        if(spec != nullptr)
        {
          string_view fmt = m_stringmem.emplace(spec->m_format).first->c_str();
          fmtPtr->setSpec(type, FormatSpec(spec->m_scale, fmt));
        }
      }

      description->format(fmtPtr);
    }

    const Category *
    Registry::findCategory(string_view category) const
    {
      absl::ReaderMutexLock l(&m_mutex);
      auto it = m_categories.find(category);
      return it == m_categories.end() ? nullptr : it->second.get();
    }

    Id
    Registry::findId(string_view category, string_view name) const
    {
      absl::ReaderMutexLock l(&m_mutex);
      auto it = m_metrics.find(std::make_tuple(category, name));
      return it == m_metrics.end() ? Id() : Id(it->second.get());
    }

    std::vector< const Category * >
    Registry::getAll() const
    {
      absl::ReaderMutexLock l(&m_mutex);

      std::vector< const Category * > result;
      result.reserve(m_categories.size());

      std::transform(m_categories.begin(), m_categories.end(),
                     std::back_inserter(result),
                     [](const auto &x) { return x.second.get(); });

      return result;
    }

    struct PublisherHelper
    {
      using SampleCache = std::map< std::shared_ptr< Publisher >, Sample >;

      static void
      updateSampleCache(SampleCache &cache,
                        const std::shared_ptr< Publisher > &publisher,
                        const SampleGroup< double > &doubleGroup,
                        const SampleGroup< int > &intGroup,
                        const absl::Time &timeStamp)
      {
        auto it = cache.find(publisher);
        if(it == cache.end())
        {
          Sample sample;
          sample.sampleTime(timeStamp);
          it = cache.emplace(publisher, sample).first;
        }
        it->second.pushGroup(doubleGroup);
        it->second.pushGroup(intGroup);
      }

      struct CollectResult
      {
        Records records;
        absl::Duration samplePeriod;
      };

      static CollectResult
      collect(Manager &manager, const Category *category,
              const absl::Duration &now, bool clear)
          EXCLUSIVE_LOCKS_REQUIRED(manager.m_mutex)
      {
        // Collect records from the repo.
        const Records result = clear
            ? Records(manager.m_doubleRepo.collectAndClear(category),
                      manager.m_intRepo.collectAndClear(category))
            : Records(manager.m_doubleRepo.collect(category),
                      manager.m_intRepo.collect(category));

        // Get the time since last reset, and clear if needed.
        auto it = manager.m_resetTimes.find(category);
        if(it == manager.m_resetTimes.end())
        {
          if(clear)
          {
            manager.m_resetTimes.emplace(category, now);
          }
          return {result, now - manager.m_createTime};
        }

        auto tmp = now - it->second;
        if(clear)
        {
          it->second = now;
        }
        return {result, tmp};
      }

      template < typename Type >
      using RecordBuffer = std::vector<
          std::shared_ptr< std::vector< TaggedRecords< Type > > > >;

      template < typename CategoryIterator >
      static void
      publish(Manager &manager, const CategoryIterator &categoriesBegin,
              const CategoryIterator &categoriesEnd, bool clear)
      {
        if(categoriesBegin == categoriesEnd)
        {
          return;
        }

        RecordBuffer< double > doubleRecordBuffer;
        RecordBuffer< int > intRecordBuffer;

        SampleCache sampleCache;

        absl::Time timeStamp = absl::Now();
        absl::Duration now   = absl::Now() - absl::UnixEpoch();
        {
          // 1.
          absl::WriterMutexLock publishGuard(&manager.m_publishLock);
          // 2.
          absl::WriterMutexLock propertiesGuard(&manager.m_mutex);

          // Build the 'sampleCache' by iterating over the categories and
          // collecting records for those categories.
          for(CategoryIterator catIt = categoriesBegin; catIt != categoriesEnd;
              ++catIt)
          {
            if(!(*catIt)->enabled())
            {
              continue;
            }
            // Collect the metrics.
            auto result         = collect(manager, *catIt, now, clear);
            const auto &records = result.records;

            // If there are no collected records then this category can be
            // ignored.
            if(records.doubleRecords.empty() && records.intRecords.empty())
            {
              continue;
            }

            if(result.samplePeriod == absl::Duration())
            {
              std::cerr << "Invalid elapsed time interval of 0 for "
                           "published metrics.";
              result.samplePeriod += absl::Nanoseconds(1);
            }

            // Append the collected records to the buffer of records.
            auto dRecords =
                std::make_shared< DoubleRecords >(records.doubleRecords);
            doubleRecordBuffer.push_back(dRecords);
            SampleGroup< double > doubleGroup(
                absl::Span< const TaggedRecords< double > >(*dRecords),
                result.samplePeriod);

            auto iRecords = std::make_shared< IntRecords >(records.intRecords);
            intRecordBuffer.push_back(iRecords);
            SampleGroup< int > intGroup(
                absl::Span< const TaggedRecords< int > >(*iRecords),
                result.samplePeriod);

            std::for_each(manager.m_publishers.globalBegin(),
                          manager.m_publishers.globalEnd(),
                          [&](const auto &ptr) {
                            updateSampleCache(sampleCache, ptr, doubleGroup,
                                              intGroup, timeStamp);
                          });

            std::for_each(manager.m_publishers.lowerBound(*catIt),
                          manager.m_publishers.upperBound(*catIt),
                          [&](const auto &val) {
                            updateSampleCache(sampleCache, val.second,
                                              doubleGroup, intGroup, timeStamp);
                          });
          }
        }

        for(auto &entry : sampleCache)
        {
          Publisher *publisher = entry.first.get();

          publisher->publish(entry.second);
        }
      }
    };

    Sample
    Manager::collectSample(Records &records,
                           absl::Span< const Category * > categories,
                           bool clear)
    {
      absl::Time timeStamp = absl::Now();
      absl::Duration now   = timeStamp - absl::UnixEpoch();

      Sample sample;
      sample.sampleTime(timeStamp);

      // Use a tuple to hold 'references' to the collected records
      using SampleDescription = std::tuple< size_t, size_t, absl::Duration >;
      std::vector< SampleDescription > dSamples;
      std::vector< SampleDescription > iSamples;
      dSamples.reserve(categories.size());
      iSamples.reserve(categories.size());

      // 1
      absl::WriterMutexLock publishGuard(&m_publishLock);
      // 2
      absl::WriterMutexLock propertiesGuard(&m_mutex);

      for(const Category *const category : categories)
      {
        if(!category->enabled())
        {
          continue;
        }

        size_t dBeginIndex = records.doubleRecords.size();
        size_t iBeginIndex = records.intRecords.size();

        // Collect the metrics.
        auto collectRes = PublisherHelper::collect(*this, category, now, clear);
        DoubleRecords catDRecords = collectRes.records.doubleRecords;
        IntRecords catIRecords    = collectRes.records.intRecords;

        absl::Duration elapsedTime = collectRes.samplePeriod;

        records.doubleRecords.insert(records.doubleRecords.end(),
                                     catDRecords.begin(), catDRecords.end());
        records.intRecords.insert(records.intRecords.end(), catIRecords.begin(),
                                  catIRecords.end());

        size_t dSize = records.doubleRecords.size() - dBeginIndex;
        size_t iSize = records.intRecords.size() - iBeginIndex;

        // If there are no collected records then this category can be ignored.
        if(dSize != 0)
        {
          dSamples.emplace_back(dBeginIndex, dSize, elapsedTime);
        }
        if(iSize != 0)
        {
          iSamples.emplace_back(iBeginIndex, iSize, elapsedTime);
        }
      }

      // Now that we have all the records, we can build our sample
      for(const SampleDescription &s : dSamples)
      {
        sample.pushGroup(&records.doubleRecords[std::get< 0 >(s)],
                         std::get< 1 >(s), std::get< 2 >(s));
      }

      for(const SampleDescription &s : iSamples)
      {
        sample.pushGroup(&records.intRecords[std::get< 0 >(s)],
                         std::get< 1 >(s), std::get< 2 >(s));
      }

      return sample;
    }

    void
    Manager::publish(absl::Span< const Category * > categories, bool clear)
    {
      PublisherHelper::publish(*this, categories.begin(), categories.end(),
                               clear);
    }

    void
    Manager::publish(const std::set< const Category * > &categories, bool clear)
    {
      PublisherHelper::publish(*this, categories.begin(), categories.end(),
                               clear);
    }

    Manager *DefaultManager::m_manager = nullptr;

    struct PublisherSchedulerData
    {
      util::Mutex m_mutex;
      thread::Scheduler::Handle m_handle GUARDED_BY(m_mutex);
      std::set< const Category * > m_categories GUARDED_BY(m_mutex);

      bool m_default GUARDED_BY(m_mutex){false};
      std::set< const Category * > m_nonDefaultCategories GUARDED_BY(m_mutex);

      PublisherSchedulerData() : m_handle(thread::Scheduler::INVALID_HANDLE)
      {
      }
    };

    // Reverts a publisher scheduler back to its default state
    class PublisherSchedulerGuard
    {
      PublisherScheduler *m_scheduler;

     public:
      PublisherSchedulerGuard(PublisherScheduler *scheduler)
          : m_scheduler(scheduler)
      {
      }

      ~PublisherSchedulerGuard()
      {
        if(m_scheduler != nullptr)
        {
          for(auto &repeat : m_scheduler->m_repeaters)
          {
            if(repeat.second->m_handle != thread::Scheduler::INVALID_HANDLE)
            {
              m_scheduler->m_scheduler.cancelRepeat(repeat.second->m_handle);
            }
          }

          m_scheduler->m_defaultInterval = absl::Duration();
          m_scheduler->m_repeaters.clear();
          m_scheduler->m_categories.clear();
        }
      }

      void
      release()
      {
        m_scheduler = nullptr;
      }
    };

    void
    PublisherScheduler::publish(
        const std::shared_ptr< PublisherSchedulerData > &data) const
    {
      util::Lock l(&data->m_mutex);
      if(data->m_default)
      {
        m_manager->publishAllExcluding(data->m_nonDefaultCategories);
      }
      else if(!data->m_categories.empty())
      {
        m_manager->publish(data->m_categories);
      }
    }

    void
    PublisherScheduler::cancel(Categories::iterator it)
    {
      assert(it != m_categories.end());
      auto repeatIt = m_repeaters.find(it->second);
      assert(repeatIt != m_repeaters.end());

      const Category *category = it->first;
      m_categories.erase(it);
      auto data = repeatIt->second;

      util::Lock l(&data->m_mutex);
      assert(data->m_categories.find(category) != data->m_categories.end());
      data->m_categories.erase(category);

      if(!data->m_default)
      {
        if(data->m_categories.empty())
        {
          m_scheduler.cancelRepeat(data->m_handle);
          m_repeaters.erase(repeatIt);
        }

        if(m_defaultInterval != absl::Duration())
        {
          auto defaultIntervalIt = m_repeaters.find(m_defaultInterval);
          assert(defaultIntervalIt != m_repeaters.end());

          auto &defaultRepeater = defaultIntervalIt->second;
          util::Lock lock(&defaultRepeater->m_mutex);
          defaultRepeater->m_nonDefaultCategories.erase(category);
        }
      }
    }

    bool
    PublisherScheduler::cancelDefault()
    {
      if(m_defaultInterval == absl::Duration())
      {
        return false;
      }

      absl::Duration interval = m_defaultInterval;
      m_defaultInterval       = absl::Duration();

      auto repeatIt = m_repeaters.find(interval);
      assert(repeatIt != m_repeaters.end());
      auto data = repeatIt->second;

      util::Lock l(&data->m_mutex);

      if(data->m_categories.empty())
      {
        assert(data->m_handle != thread::Scheduler::INVALID_HANDLE);
        m_scheduler.cancelRepeat(data->m_handle);
        m_repeaters.erase(repeatIt);
      }
      else
      {
        data->m_default = false;
        data->m_nonDefaultCategories.clear();
      }
      return true;
    }

    void
    PublisherScheduler::schedule(const Category *category,
                                 absl::Duration interval)
    {
      assert(absl::Seconds(0) < interval);

      util::Lock l(&m_mutex);

      auto catIt = m_categories.find(category);
      if(catIt != m_categories.end())
      {
        if(catIt->second == interval)
        {
          return;
        }
        cancel(catIt);
      }

      // Make a guard, so if something throws, the scheduler is reset to a
      // somewhat "sane" state (no metrics).
      PublisherSchedulerGuard guard(this);

      m_categories.emplace(category, interval);
      auto repeatIt = m_repeaters.find(interval);
      std::shared_ptr< PublisherSchedulerData > data;

      // Create a new 'ClockData' object if one does not exist for the
      // 'interval', otherwise update the existing 'data'.
      if(repeatIt == m_repeaters.end())
      {
        data = std::make_shared< PublisherSchedulerData >();
        util::Lock lock(&data->m_mutex);
        data->m_categories.insert(category);
        m_repeaters.emplace(interval, data);
        data->m_handle = m_scheduler.scheduleRepeat(
            interval, std::bind(&PublisherScheduler::publish, this, data));
      }
      else
      {
        data = repeatIt->second;
        util::Lock lock(&data->m_mutex);
        data->m_categories.insert(category);
      }

      // If this isn't being added to the default schedule, then add to the set
      // of non-default categories in the default schedule.

      util::Lock dataLock(&data->m_mutex);
      if(!data->m_default && m_defaultInterval != absl::Duration())
      {
        auto defaultIntervalIt = m_repeaters.find(m_defaultInterval);
        assert(defaultIntervalIt != m_repeaters.end());

        auto &defaultInterval = defaultIntervalIt->second;
        util::Lock lock(&defaultInterval->m_mutex);
        defaultInterval->m_nonDefaultCategories.insert(category);
      }

      guard.release();
    }

    void
    PublisherScheduler::setDefault(absl::Duration interval)
    {
      assert(absl::Seconds(0) < interval);
      util::Lock l(&m_mutex);

      // If its already this interval, return early.
      if(interval == m_defaultInterval)
      {
        return;
      }

      cancelDefault();
      m_defaultInterval = interval;

      // Make a guard, so if something throws, the scheduler is reset to a
      // somewhat "sane" state (no metrics).
      PublisherSchedulerGuard guard(this);

      std::shared_ptr< PublisherSchedulerData > data;
      auto repeatIt = m_repeaters.find(interval);
      if(repeatIt == m_repeaters.end())
      {
        data = std::make_shared< PublisherSchedulerData >();
        m_repeaters.emplace(interval, data);
      }
      else
      {
        data = repeatIt->second;
      }

      util::Lock lock(&data->m_mutex);
      data->m_default = true;

      auto cIt = m_categories.begin();
      for(; cIt != m_categories.end(); ++cIt)
      {
        if(cIt->second != interval)
        {
          data->m_nonDefaultCategories.insert(cIt->first);
        }
      }

      if(data->m_handle == thread::Scheduler::INVALID_HANDLE)
      {
        data->m_handle = m_scheduler.scheduleRepeat(
            interval, std::bind(&PublisherScheduler::publish, this, data));
      }

      guard.release();
    }

    bool
    PublisherScheduler::cancel(const Category *category)
    {
      util::Lock l(&m_mutex);

      auto it = m_categories.find(category);
      if(it == m_categories.end())
      {
        // This category has no specific schedule.
        return false;
      }
      cancel(it);

      return true;
    }

    bool
    PublisherScheduler::clearDefault()
    {
      util::Lock l(&m_mutex);
      return cancelDefault();
    }

    void
    PublisherScheduler::cancelAll()
    {
      util::Lock l(&m_mutex);
      for(auto &repeat : m_repeaters)
      {
        util::Lock dataLock(&repeat.second->m_mutex);
        m_scheduler.cancelRepeat(repeat.second->m_handle, true);
      }

      m_defaultInterval = absl::Duration();
      m_repeaters.clear();
      m_categories.clear();
    }

    nonstd::optional< absl::Duration >
    PublisherScheduler::find(const Category *category) const
    {
      util::Lock l(&m_mutex);
      auto it = m_categories.find(category);

      if(it == m_categories.end())
      {
        return {};
      }

      return it->second;
    }

    nonstd::optional< absl::Duration >
    PublisherScheduler::getDefault() const
    {
      util::Lock l(&m_mutex);

      if(m_defaultInterval == absl::Duration())
      {
        return {};
      }

      return m_defaultInterval;
    }

    std::vector< std::pair< const Category *, absl::Duration > >
    PublisherScheduler::getAll() const
    {
      util::Lock l(&m_mutex);
      std::vector< std::pair< const Category *, absl::Duration > > result;
      result.reserve(m_categories.size());
      std::copy(m_categories.begin(), m_categories.end(),
                std::back_inserter(result));
      return result;
    }

  }  // namespace metrics
}  // namespace llarp

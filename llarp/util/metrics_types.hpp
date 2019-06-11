#ifndef LLARP_METRICS_TYPES_HPP
#define LLARP_METRICS_TYPES_HPP

#include <util/printer.hpp>
#include <util/string_view.hpp>
#include <util/threading.hpp>
#include <util/types.hpp>

#include <absl/types/span.h>
#include <absl/types/optional.h>
#include <cstring>
#include <iosfwd>
#include <memory>
#include <set>
#include <vector>

namespace llarp
{
  namespace metrics
  {
    struct Publication
    {
      enum class Type : byte_t
      {
        Unspecified = 0,  // no associated metric type
        Total,            // sum of seen values in the measurement period
        Count,            // count of seen events
        Min,              // Minimum value
        Max,              // Max value
        Avg,              // total / count
        Rate,             // total per second
        RateCount         // count per second
      };

      enum
      {
        MaxSize = static_cast< byte_t >(Type::RateCount) + 1
      };

      static string_view
      repr(Type val);

      static std::ostream &
      print(std::ostream &stream, Type val);
    };

    struct FormatSpec
    {
      float m_scale;
      const char *m_format;

      static constexpr char DEFAULT_FORMAT[] = "%f";

      constexpr FormatSpec() : m_scale(1.0), m_format(DEFAULT_FORMAT)
      {
      }

      constexpr FormatSpec(float scale, const char *format)
          : m_scale(scale), m_format(format)
      {
      }

      static std::ostream &
      format(std::ostream &stream, double data, const FormatSpec &spec);
    };

    inline bool
    operator==(const FormatSpec &lhs, const FormatSpec &rhs)
    {
      return lhs.m_scale == rhs.m_scale
          && std::strcmp(lhs.m_format, rhs.m_format) == 0;
    }

    struct Format
    {
      using Spec = absl::optional< FormatSpec >;

      std::array< Spec, Publication::MaxSize > m_specs;

      constexpr Format() : m_specs()
      {
      }

      void
      setSpec(Publication::Type pub, const FormatSpec &spec)
      {
        m_specs[static_cast< size_t >(pub)].emplace(spec);
      }

      constexpr void
      clear()
      {
        m_specs = decltype(m_specs)();
      }

      constexpr const FormatSpec *
      specFor(Publication::Type val) const
      {
        const auto &spec = m_specs[static_cast< size_t >(val)];
        return spec ? &spec.value() : nullptr;
      }
    };

    inline bool
    operator==(const Format &lhs, const Format &rhs)
    {
      return lhs.m_specs == rhs.m_specs;
    }

    struct CategoryContainer;

    /// Represents a category of grouped metrics
    class Category
    {
      const char *m_name;
      std::atomic_bool m_enabled;
      CategoryContainer *m_container;

     public:
      Category(const char *name, bool enabled = true)
          : m_name(name), m_enabled(enabled), m_container(nullptr)
      {
      }

      ~Category();

      void
      enabled(bool flag);

      void
      registerContainer(CategoryContainer *container);

      const std::atomic_bool &
      enabledRaw() const
      {
        return m_enabled;
      }

      const char *
      name() const
      {
        return m_name;
      }

      bool
      enabled() const
      {
        return m_enabled;
      }

      std::ostream &
      print(std::ostream &stream, int level, int spaces) const;
    };

    inline std::ostream &
    operator<<(std::ostream &stream, const Category &c)
    {
      return c.print(stream, -1, -1);
    }

    struct CategoryContainer
    {
      bool m_enabled;
      const Category *m_category;
      CategoryContainer *m_nextCategory;

      constexpr void
      clear()
      {
        m_enabled      = false;
        m_category     = nullptr;
        m_nextCategory = nullptr;
      }
    };

    class Description
    {
      mutable util::Mutex m_mutex;

      const Category *m_category GUARDED_BY(m_mutex);
      const char *m_name GUARDED_BY(m_mutex);
      Publication::Type m_type GUARDED_BY(m_mutex);
      std::shared_ptr< Format > m_format GUARDED_BY(m_mutex);

      Description(const Description &) = delete;
      Description &
      operator=(const Description &) = delete;

     public:
      Description(const Category *category, const char *name)
          : m_category(category)
          , m_name(name)
          , m_type(Publication::Type::Unspecified)
      {
      }

      void
      category(const Category *c)
      {
        util::Lock l(&m_mutex);
        m_category = c;
      }

      const Category *
      category() const
      {
        util::Lock l(&m_mutex);
        return m_category;
      }

      void
      name(const char *n)
      {
        util::Lock l(&m_mutex);
        m_name = n;
      }

      const char *
      name() const
      {
        util::Lock l(&m_mutex);
        return m_name;
      }

      void
      type(Publication::Type t)
      {
        util::Lock l(&m_mutex);
        m_type = t;
      }

      Publication::Type
      type() const
      {
        util::Lock l(&m_mutex);
        return m_type;
      }

      void
      format(const std::shared_ptr< Format > &f)
      {
        util::Lock l(&m_mutex);
        m_format = f;
      }

      std::shared_ptr< Format >
      format() const
      {
        util::Lock l(&m_mutex);
        return m_format;
      }

      std::string
      toString() const;

      std::ostream &
      print(std::ostream &stream) const;
    };

    inline std::ostream &
    operator<<(std::ostream &stream, const Description &d)
    {
      return d.print(stream);
    }

    /// A metric id is what we will actually deal with in terms of metrics, in
    /// order to make things like static initialisation cleaner.
    class Id
    {
      const Description *m_description;

     public:
      constexpr Id() : m_description(nullptr)
      {
      }

      constexpr Id(const Description *description) : m_description(description)
      {
      }

      constexpr const Description *&
      description()
      {
        return m_description;
      }

      constexpr const Description *const &
      description() const
      {
        return m_description;
      }

      bool
      valid() const noexcept
      {
        return m_description != nullptr;
      }

      explicit operator bool() const noexcept
      {
        return valid();
      }

      const Category *
      category() const
      {
        assert(valid());
        return m_description->category();
      }

      const char *
      categoryName() const
      {
        assert(valid());
        return m_description->category()->name();
      }

      const char *
      metricName() const
      {
        assert(valid());
        return m_description->name();
      }

      std::string
      toString() const
      {
        if(m_description)
        {
          return m_description->toString();
          ;
        }
        else
        {
          return "INVALID_METRIC";
        }
      }

      std::ostream &
      print(std::ostream &stream, int, int) const
      {
        if(m_description)
        {
          stream << *m_description;
        }
        else
        {
          stream << "INVALID_METRIC";
        }

        return stream;
      }
    };

    inline bool
    operator==(const Id &lhs, const Id &rhs)
    {
      return lhs.description() == rhs.description();
    }

    inline bool
    operator<(const Id &lhs, const Id &rhs)
    {
      return lhs.description() < rhs.description();
    }

    inline std::ostream &
    operator<<(std::ostream &stream, const Id &id)
    {
      return id.print(stream, -1, -1);
    }

    // clang-format off
    // Forwarding class to specialise for metric types
    template<typename Type>
    struct RecordMax {
    };

    template<>
    struct RecordMax<double> {
      static constexpr double min() { return std::numeric_limits< double >::infinity(); }
      static constexpr double max() { return -std::numeric_limits< double >::infinity(); }
    };

    template<>
    struct RecordMax<int> {
      static constexpr int min() { return std::numeric_limits< int >::max(); }
      static constexpr int max() { return std::numeric_limits< int >::min(); }
    };
    // clang-format on

    template < typename Type >
    class Record
    {
      Id m_id;
      size_t m_count;
      Type m_total;
      Type m_min;
      Type m_max;

     public:
      // clang-format off
      static constexpr Type DEFAULT_MIN() { return RecordMax<Type>::min(); };
      static constexpr Type DEFAULT_MAX() { return RecordMax<Type>::max(); };
      // clang-format on

      Record()
          : m_id()
          , m_count(0)
          , m_total(0.0)
          , m_min(DEFAULT_MIN())
          , m_max(DEFAULT_MAX())
      {
      }

      explicit Record(const Id &id)
          : m_id(id)
          , m_count(0)
          , m_total()
          , m_min(DEFAULT_MIN())
          , m_max(DEFAULT_MAX())
      {
      }

      Record(const Id &id, size_t count, double total, double min, double max)
          : m_id(id), m_count(count), m_total(total), m_min(min), m_max(max)
      {
      }

      // clang-format off
      const Id& id() const { return m_id; }
      Id& id()             { return m_id; }

      size_t count() const { return m_count; }
      size_t& count()      { return m_count; }

      Type total() const { return m_total; }
      Type& total()      { return m_total; }

      Type min() const { return m_min; }
      Type& min()      { return m_min; }

      Type max() const { return m_max; }
      Type& max()      { return m_max; }
      // clang-format on

      void
      clear()
      {
        m_count = 0;
        m_total = 0;
        m_min   = DEFAULT_MIN();
        m_max   = DEFAULT_MAX();
      }

      std::ostream &
      print(std::ostream &stream, int level, int spaces) const
      {
        Printer printer(stream, level, spaces);
        printer.printAttribute("id", m_id);
        printer.printAttribute("count", m_count);
        printer.printAttribute("total", m_total);
        printer.printAttribute("min", m_min);
        printer.printAttribute("max", m_max);

        return stream;
      }
    };

    template < typename Type >
    inline std::ostream &
    operator<<(std::ostream &stream, const Record< Type > &rec)
    {
      return rec.print(stream, -1, -1);
    }

    template < typename Type >
    inline bool
    operator==(const Record< Type > &lhs, const Record< Type > &rhs)
    {
      return (lhs.id() == rhs.id() && lhs.count() == rhs.count()
              && lhs.total() == rhs.total() && lhs.min() == rhs.min()
              && lhs.max() == rhs.max());
    }

    template < typename Type >
    inline bool
    operator!=(const Record< Type > &lhs, const Record< Type > &rhs)
    {
      return !(lhs == rhs);
    }

    template < typename Type >
    class SampleGroup
    {
      absl::Span< const Record< Type > > m_records;
      absl::Duration m_samplePeriod;

     public:
      using RecordType = Record< Type >;
      using const_iterator =
          typename absl::Span< const RecordType >::const_iterator;

      SampleGroup() : m_records(), m_samplePeriod()
      {
      }

      SampleGroup(const RecordType *records, size_t size,
                  absl::Duration samplePeriod)
          : m_records(records, size), m_samplePeriod(samplePeriod)
      {
      }

      SampleGroup(const absl::Span< const RecordType > &records,
                  absl::Duration samplePeriod)
          : m_records(records), m_samplePeriod(samplePeriod)
      {
      }

      // clang-format off
      void samplePeriod(absl::Duration duration) { m_samplePeriod = duration; }
      absl::Duration samplePeriod() const { return m_samplePeriod; }

      void records(absl::Span<const RecordType> recs) { m_records = recs; }
      absl::Span<const RecordType> records() const { return m_records; }

      bool empty() const { return m_records.empty(); }
      size_t size() const { return m_records.size(); }

      const_iterator begin() const { return m_records.begin(); }
      const_iterator end() const { return m_records.end(); }
      // clang-format on

      std::ostream &
      print(std::ostream &stream, int level, int spaces) const
      {
        Printer::PrintFunction< absl::Duration > durationPrinter =
            [](std::ostream &stream, const absl::Duration &duration, int,
               int) -> std::ostream & {
          stream << duration;
          return stream;
        };
        Printer printer(stream, level, spaces);
        printer.printAttribute("records", m_records);
        printer.printForeignAttribute("samplePeriod", m_samplePeriod,
                                      durationPrinter);

        return stream;
      }
    };

    template < typename Type >
    inline std::ostream &
    operator<<(std::ostream &stream, const SampleGroup< Type > &group)
    {
      return group.print(stream, -1, -1);
    }

    template < typename Type >
    inline bool
    operator==(const SampleGroup< Type > &lhs, const SampleGroup< Type > &rhs)
    {
      return lhs.records() == rhs.records()
          && lhs.samplePeriod() == rhs.samplePeriod();
    }

    template < typename Type >
    class Sample
    {
      absl::Time m_sampleTime;
      std::vector< SampleGroup< Type > > m_samples;
      size_t m_recordCount;

     public:
      using const_iterator =
          typename std::vector< SampleGroup< Type > >::const_iterator;

      Sample() : m_sampleTime(), m_recordCount(0)
      {
      }

      // clang-format off
      void sampleTime(const absl::Time& time) { m_sampleTime = time; }
      absl::Time sampleTime() const { return m_sampleTime; }

      void pushGroup(const SampleGroup<Type>& group) {
        if (!group.empty()) {
          m_samples.push_back(group);
          m_recordCount += group.size();
        }
      }

      void pushGroup(const Record<Type> *records, size_t size, absl::Duration duration) {
        if (size != 0) {
          m_samples.emplace_back(records, size, duration);
          m_recordCount += size;
        }
      }

      void pushGroup(const absl::Span< const Record<Type> > &records,absl::Duration duration) {
        if (!records.empty()) {
          m_samples.emplace_back(records, duration);
          m_recordCount += records.size();
        }
      }

      void clear() {
        m_samples.clear();
        m_recordCount = 0;
      }

      const SampleGroup<Type>& group(size_t index) {
        assert(index < m_samples.size());
        return m_samples[index];
      }

      const_iterator begin() const { return m_samples.begin(); }
      const_iterator end() const { return m_samples.end(); }

      size_t groupCount() const { return m_samples.size(); }
      size_t recordCount() const { return m_recordCount; }
      // clang-format on
    };
  }  // namespace metrics
}  // namespace llarp

#endif

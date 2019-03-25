#include <util/metrics_types.hpp>

#include <util/printer.hpp>

namespace llarp
{
  namespace metrics
  {
    std::ostream &
    FormatSpec::format(std::ostream &stream, double data,
                       const FormatSpec &format)
    {
      static constexpr size_t INIT_SIZE = 32;

      char buf[INIT_SIZE] = {0};
      int rc = snprintf(buf, INIT_SIZE, format.m_format, data * format.m_scale);

      if(rc < 0)
      {
        stream << "Bad format " << format.m_format << " applied to " << data;
        return stream;
      }

      if(static_cast< size_t >(rc) < INIT_SIZE)
      {
        stream << buf;
        return stream;
      }

      std::vector< char > vec(rc + 1);
      rc = snprintf(vec.data(), vec.size(), format.m_format,
                    data * format.m_scale);

      if(static_cast< size_t >(rc) > vec.size())
      {
        stream << "Bad format " << format.m_format << " applied to " << data;
        return stream;
      }
      else
      {
        stream << vec.data();
        return stream;
      }
    }

    string_view
    Publication::repr(Type val)
    {
      switch(val)
      {
        case Type::Unspecified:
          return "Unspecified";
        case Type::Total:
          return "Total";
        case Type::Count:
          return "Count";
        case Type::Min:
          return "Min";
        case Type::Max:
          return "Max";
        case Type::Avg:
          return "Avg";
        case Type::Rate:
          return "Rate";
        case Type::RateCount:
          return "RateCount";
      }
    }

    std::ostream &
    Publication::print(std::ostream &stream, Type val)
    {
      stream << repr(val);
      return stream;
    }

    Category::~Category()
    {
      while(m_container)
      {
        auto next = m_container->m_nextCategory;
        m_container->clear();
        m_container = next;
      }
    }

    void
    Category::enabled(bool val)
    {
      // sync point
      if(m_enabled != val)
      {
        auto cont = m_container;
        while(cont)
        {
          cont->m_enabled = val;
          cont            = cont->m_nextCategory;
        }

        m_enabled = val;
      }
    }

    void
    Category::registerContainer(CategoryContainer *container)
    {
      container->m_enabled      = m_enabled;
      container->m_category     = this;
      container->m_nextCategory = m_container;
      m_container               = container;
    }

    std::ostream &
    Category::print(std::ostream &stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("name", m_name);
      printer.printAttribute("enabled",
                             m_enabled.load(std::memory_order_relaxed));

      return stream;
    }

    std::ostream &
    Description::print(std::ostream &stream) const
    {
      util::Lock l(&m_mutex);

      stream << m_category->name() << '.' << m_name;

      return stream;
    }

    const double Record::DEFAULT_MIN = std::numeric_limits< double >::max() * 2;
    const double Record::DEFAULT_MAX =
        std::numeric_limits< double >::max() * -2;

    std::ostream &
    Record::print(std::ostream &stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("id", m_id);
      printer.printAttribute("count", m_count);
      printer.printAttribute("total", m_total);
      printer.printAttribute("min", m_min);
      printer.printAttribute("max", m_max);

      return stream;
    }

    std::ostream &
    SampleGroup::print(std::ostream &stream, int level, int spaces) const
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
  }  // namespace metrics

}  // namespace llarp

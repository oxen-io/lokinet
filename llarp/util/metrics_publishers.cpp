#include <util/metrics_publishers.hpp>

#include <iostream>

namespace llarp
{
  namespace metrics
  {
    namespace
    {
      void
      formatValue(std::ostream &stream, size_t value,
                  const FormatSpec *formatSpec)
      {
        if(formatSpec)
        {
          FormatSpec::format(stream, (double)value, *formatSpec);
        }
        else
        {
          stream << value;
        }
      }

      void
      formatValue(std::ostream &stream, int value, const FormatSpec *formatSpec)
      {
        if(formatSpec)
        {
          FormatSpec::format(stream, (double)value, *formatSpec);
        }
        else
        {
          stream << value;
        }
      }

      void
      formatValue(std::ostream &stream, double value,
                  const FormatSpec *formatSpec)
      {
        if(formatSpec)
        {
          FormatSpec::format(stream, value, *formatSpec);
        }
        else
        {
          stream << value;
        }
      }
      void
      formatValue(std::ostream &stream, const Record &record,
                  double elapsedTime, Publication::Type publicationType,
                  const FormatSpec *formatSpec)
      {
        switch(publicationType)
        {
          case Publication::Type::Unspecified:
          {
            assert(false && "Invalid publication type");
          }
          break;
          case Publication::Type::Total:
          {
            formatValue(stream, record.total(), formatSpec);
          }
          break;
          case Publication::Type::Count:
          {
            formatValue(stream, record.count(), formatSpec);
          }
          break;
          case Publication::Type::Min:
          {
            formatValue(stream, record.min(), formatSpec);
          }
          break;
          case Publication::Type::Max:
          {
            formatValue(stream, record.max(), formatSpec);
          }
          break;
          case Publication::Type::Avg:
          {
            formatValue(stream, record.total() / record.count(), formatSpec);
          }
          break;
          case Publication::Type::Rate:
          {
            formatValue(stream, record.total() / elapsedTime, formatSpec);
          }
          break;
          case Publication::Type::RateCount:
          {
            formatValue(stream, record.count() / elapsedTime, formatSpec);
          }
          break;
        }
      }

      void
      publishRecord(std::ostream &stream, const Record &record,
                    double elapsedTime)
      {
        auto publicationType = record.id().description()->type();
        std::shared_ptr< const Format > format =
            record.id().description()->format();

        stream << "\t\t" << record.id() << " [ ";

        if(publicationType != Publication::Type::Unspecified)
        {
          stream << Publication::repr(publicationType) << " = ";
          const FormatSpec *formatSpec =
              format ? format->specFor(publicationType) : nullptr;

          formatValue(stream, record, elapsedTime, publicationType, formatSpec);
        }
        else
        {
          const FormatSpec *countSpec = nullptr;
          const FormatSpec *totalSpec = nullptr;
          const FormatSpec *minSpec   = nullptr;
          const FormatSpec *maxSpec   = nullptr;

          if(format)
          {
            countSpec = format->specFor(Publication::Type::Count);
            totalSpec = format->specFor(Publication::Type::Total);
            minSpec   = format->specFor(Publication::Type::Min);
            maxSpec   = format->specFor(Publication::Type::Max);
          }
          stream << "count = ";
          formatValue(stream, record.count(), countSpec);
          stream << ", total = ";
          formatValue(stream, record.total(), totalSpec);
          if(Record::DEFAULT_MIN == record.min())
          {
            stream << ", min = undefined";
          }
          else
          {
            stream << ", min = ";
            formatValue(stream, record.min(), minSpec);
          }
          if(Record::DEFAULT_MAX == record.max())
          {
            stream << ", max = undefined";
          }
          else
          {
            stream << ", max = ";
            formatValue(stream, record.max(), maxSpec);
          }
        }
        stream << " ]\n";
      }

    }  // namespace
    void
    StreamPublisher::publish(const Sample &values)
    {
      if(values.recordCount() > 0)
      {
        m_stream << values.sampleTime() << " " << values.recordCount()
                 << " Records\n";

        auto gIt  = values.begin();
        auto prev = values.begin();
        for(; gIt != values.end(); ++gIt)
        {
          const double elapsedTime = absl::ToDoubleSeconds(gIt->samplePeriod());

          if(gIt == prev || gIt->samplePeriod() != prev->samplePeriod())
          {
            m_stream << "\tElapsed Time: " << elapsedTime << "s\n";
          }

          for(const auto &record : *gIt)
          {
            publishRecord(m_stream, record, elapsedTime);
          }
          prev = gIt;
        }
      }
    }
  }  // namespace metrics
}  // namespace llarp

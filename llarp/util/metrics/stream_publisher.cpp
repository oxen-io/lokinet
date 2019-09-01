#include <util/metrics/stream_publisher.hpp>

#include <fstream>
#include <iostream>
#include <iomanip>

namespace llarp
{
  namespace metrics
  {
    namespace
    {
      template < typename Value >
      void
      formatValue(std::ostream &stream, Value value,
                  const FormatSpec *formatSpec)
      {
        if(formatSpec)
        {
          FormatSpec::format(stream, static_cast< double >(value), *formatSpec);
        }
        else
        {
          stream << value;
        }
      }

      template < typename Value >
      void
      formatValue(std::ostream &stream, const Record< Value > &record,
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

      template < typename Value >
      void
      publishRecord(std::ostream &stream,
                    const TaggedRecords< Value > &taggedRecords,
                    double elapsedTime)
      {
        auto publicationType = taggedRecords.id.description()->type();
        std::shared_ptr< const Format > format =
            taggedRecords.id.description()->format();

        if(taggedRecords.data.empty())
        {
          return;
        }

        stream << "\t\t" << taggedRecords.id << " [";

        for(const auto &rec : taggedRecords.data)
        {
          stream << "\n\t\t\t";
          const auto &tags   = rec.first;
          const auto &record = rec.second;

          {
            Printer printer(stream, -1, -1);
            printer.printValue(tags);
          }

          stream << " ";

          if(publicationType != Publication::Type::Unspecified)
          {
            stream << Publication::repr(publicationType) << " = ";
            const FormatSpec *formatSpec =
                format ? format->specFor(publicationType) : nullptr;

            formatValue(stream, record, elapsedTime, publicationType,
                        formatSpec);
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
            if(Record< Value >::DEFAULT_MIN() == record.min())
            {
              stream << ", min = undefined";
            }
            else
            {
              stream << ", min = ";
              formatValue(stream, record.min(), minSpec);
            }
            if(Record< Value >::DEFAULT_MAX() == record.max())
            {
              stream << ", max = undefined";
            }
            else
            {
              stream << ", max = ";
              formatValue(stream, record.max(), maxSpec);
            }
          }
        }
        stream << "\n\t\t]\n";
      }
    }  // namespace

    void
    StreamPublisher::publish(const Sample &values)
    {
      if(values.recordCount() == 0)
      {
        // nothing to publish
        return;
      }

      m_stream << values.sampleTime() << " " << values.recordCount()
               << " Records\n";

      auto gIt  = values.begin();
      auto prev = values.begin();
      for(; gIt != values.end(); ++gIt)
      {
        const double elapsedTime = absl::ToDoubleSeconds(samplePeriod(*gIt));

        if(gIt == prev || samplePeriod(*gIt) != samplePeriod(*prev))
        {
          m_stream << "\tElapsed Time: " << elapsedTime << "s\n";
        }

        absl::visit(
            [&](const auto &x) {
              for(const auto &record : x)
              {
                publishRecord(m_stream, record, elapsedTime);
              }
            },
            *gIt);

        prev = gIt;
      }
    }
  }  // namespace metrics
}  // namespace llarp

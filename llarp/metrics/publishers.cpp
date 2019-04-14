#include <metrics/publishers.hpp>

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

      void
      formatValue(nlohmann::json &result, const Record &record,
                  double elapsedTime, Publication::Type publicationType)
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
            result["total"] = record.total();
          }
          break;
          case Publication::Type::Count:
          {
            result["count"] = record.count();
          }
          break;
          case Publication::Type::Min:
          {
            result["min"] = record.min();
          }
          break;
          case Publication::Type::Max:
          {
            result["max"] = record.max();
          }
          break;
          case Publication::Type::Avg:
          {
            result["avg"] = record.total() / record.count();
          }
          break;
          case Publication::Type::Rate:
          {
            result["rate"] = record.total() / elapsedTime;
          }
          break;
          case Publication::Type::RateCount:
          {
            result["rateCount"] = record.count() / elapsedTime;
          }
          break;
        }
      }

      nlohmann::json
      recordToJson(const Record &record, double elapsedTime)
      {
        nlohmann::json result;
        result["id"] = record.id().toString();

        auto publicationType = record.id().description()->type();
        if(publicationType != Publication::Type::Unspecified)
        {
          result["publicationType"] = Publication::repr(publicationType);

          formatValue(result, record, elapsedTime, publicationType);
        }
        else
        {
          result["count"] = record.count();
          result["total"] = record.total();

          if(Record::DEFAULT_MIN != record.min())
          {
            result["min"] = record.min();
          }
          if(Record::DEFAULT_MAX == record.max())
          {
            result["max"] = record.max();
          }
        }

        return result;
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

    void
    JsonPublisher::publish(const Sample &values)
    {
      if(values.recordCount() == 0)
      {
        // nothing to publish
        return;
      }

      nlohmann::json result;
      result["sampleTime"]  = absl::UnparseFlag(values.sampleTime());
      result["recordCount"] = values.recordCount();
      auto gIt              = values.begin();
      auto prev             = values.begin();
      for(; gIt != values.end(); ++gIt)
      {
        const double elapsedTime = absl::ToDoubleSeconds(gIt->samplePeriod());

        if(gIt == prev || gIt->samplePeriod() != prev->samplePeriod())
        {
          result["elapsedTime"] = elapsedTime;
        }

        for(const auto &record : *gIt)
        {
          result["record"].emplace_back(recordToJson(record, elapsedTime));
        }
        prev = gIt;
      }

      m_publish(result);
    }

    void
    JsonPublisher::directoryPublisher(const nlohmann::json &result,
                                      const fs::path& path)
    {
      std::ofstream fstream(path.string(), std::ios_base::app);
      if(!fstream)
      {
        std::cerr << "Skipping metrics publish, " << path << " is not a file\n";
        abort();
      }

      fstream << std::setw(0) << result << '\n';
      fstream.close();
    }
  }  // namespace metrics
}  // namespace llarp

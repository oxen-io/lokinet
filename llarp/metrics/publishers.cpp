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

        stream << "\t\t" << taggedRecords.id << " [\n";

        for(const auto &rec : taggedRecords.data)
        {
          stream << "\t\t\t";
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

      nlohmann::json
      tagsToJson(const Tags &tags)
      {
        nlohmann::json result;

        std::for_each(tags.begin(), tags.end(), [&](const auto &tag) {
          absl::visit([&](const auto &t) { result[tag.first] = t; },
                      tag.second);
        });

        return result;
      }

      template < typename Value >
      nlohmann::json
      formatValue(const Record< Value > &record, const Tags &tags,
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
            return {{"tags", tagsToJson(tags)}, {"total", record.total()}};
          }
          break;
          case Publication::Type::Count:
          {
            return {{"tags", tagsToJson(tags)}, {"count", record.count()}};
          }
          break;
          case Publication::Type::Min:
          {
            return {{"tags", tagsToJson(tags)}, {"min", record.min()}};
          }
          break;
          case Publication::Type::Max:
          {
            return {{"tags", tagsToJson(tags)}, {"max", record.max()}};
          }
          break;
          case Publication::Type::Avg:
          {
            return {{"tags", tagsToJson(tags)},
                    {"avg", record.total() / record.count()}};
          }
          break;
          case Publication::Type::Rate:
          {
            return {{"tags", tagsToJson(tags)},
                    {"rate", record.total() / elapsedTime}};
          }
          break;
          case Publication::Type::RateCount:
          {
            return {{"tags", tagsToJson(tags)},
                    {"rateCount", record.count() / elapsedTime}};
          }
          break;
        }
      }

      template < typename Value >
      nlohmann::json
      recordToJson(const TaggedRecords< Value > &taggedRecord,
                   double elapsedTime)
      {
        nlohmann::json result;
        result["id"] = taggedRecord.id.toString();

        auto publicationType = taggedRecord.id.description()->type();

        for(const auto &rec : taggedRecord.data)
        {
          const auto &record = rec.second;
          if(publicationType != Publication::Type::Unspecified)
          {
            result["publicationType"] = Publication::repr(publicationType);

            result["metrics"].push_back(
                formatValue(record, rec.first, elapsedTime, publicationType));
          }
          else
          {
            nlohmann::json tmp;
            tmp["tags"]  = tagsToJson(rec.first);
            tmp["count"] = record.count();
            tmp["total"] = record.total();

            if(Record< Value >::DEFAULT_MIN() != record.min())
            {
              tmp["min"] = record.min();
            }
            if(Record< Value >::DEFAULT_MAX() == record.max())
            {
              tmp["max"] = record.max();
            }

            result["metrics"].push_back(tmp);
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
        const double elapsedTime = absl::ToDoubleSeconds(samplePeriod(*gIt));

        if(gIt == prev || samplePeriod(*gIt) != samplePeriod(*prev))
        {
          result["elapsedTime"] = elapsedTime;
        }

        absl::visit(
            [&](const auto &x) -> void {
              for(const auto &record : x)
              {
                result["record"].emplace_back(
                    recordToJson(record, elapsedTime));
              }
            },
            *gIt);

        prev = gIt;
      }

      m_publish(result);
    }

    void
    JsonPublisher::directoryPublisher(const nlohmann::json &result,
                                      const fs::path &path)
    {
      std::ofstream fstream(path.string(), std::ios_base::app);
      if(!fstream)
      {
        std::cerr << "Skipping metrics publish, " << path << " is not a file\n";
        return;
      }

      fstream << std::setw(0) << result << '\n';
      fstream.close();
    }
  }  // namespace metrics
}  // namespace llarp

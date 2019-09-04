#include <util/metrics/json_publisher.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>

namespace llarp
{
  namespace metrics
  {
    namespace
    {
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
        return {};
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

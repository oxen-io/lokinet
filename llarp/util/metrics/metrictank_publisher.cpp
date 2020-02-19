#include <util/metrics/metrictank_publisher.hpp>

#include <util/logging/logger.hpp>
#include <util/meta/variant.hpp>

#include <cstdio>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
// bzero and friends graduated from /usr/ucb*
// not too long ago
#include <strings.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#include <lmcons.h>
#endif

namespace llarp
{
  namespace metrics
  {
    namespace
    {
      nonstd::optional< std::string >
      makeStr(double d)
      {
        if(std::isnan(d) || std::isinf(d))
        {
          return {};
        }

        return std::to_string(d);
      }

      nonstd::optional< std::string >
      makeStr(int i)
      {
        if(i == std::numeric_limits< int >::min()
           || i == std::numeric_limits< int >::max())
        {
          return {};
        }

        return std::to_string(i);
      }

      template < typename Value >
      nonstd::optional< std::string >
      formatValue(const Record< Value > &record, double elapsedTime,
                  Publication::Type publicationType)
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
            return makeStr(record.total());
          }
          break;
          case Publication::Type::Count:
          {
            return std::to_string(record.count());
          }
          break;
          case Publication::Type::Min:
          {
            return makeStr(record.min());
          }
          break;
          case Publication::Type::Max:
          {
            return makeStr(record.max());
          }
          break;
          case Publication::Type::Avg:
          {
            return makeStr(static_cast< double >(record.total())
                           / static_cast< double >(record.count()));
          }
          break;
          case Publication::Type::Rate:
          {
            return makeStr(record.total() / elapsedTime);
          }
          break;
          case Publication::Type::RateCount:
          {
            return makeStr(record.count() / elapsedTime);
          }
          break;
        }
        assert(false && "Invalid publication type");
        return {};
      }

      std::string
      makeTagStr(const Tags &tags)
      {
        std::string tagStr;

        auto overloaded = util::overloaded(
            [](const std::string &str) { return str; },
            [](double d) { return std::to_string(d); },
            [](const std::int64_t i) { return std::to_string(i); });

        for(const auto &tag : tags)
        {
          absl::StrAppend(&tagStr, ";", tag.first, "=",
                          absl::visit(overloaded, tag.second));
        }
        if(!tags.empty())
        {
          absl::StrAppend(&tagStr, ";");
        }
        return tagStr;
      }

      std::string
      addName(string_view id, string_view name, const Tags &tags,
              string_view suffix)
      {
        return absl::StrCat(id, ".", name, makeTagStr(tags), suffix);
      }

      constexpr bool
      isValid(int val)
      {
        return val != std::numeric_limits< int >::min()
            && val != std::numeric_limits< int >::max();
      }

      constexpr bool
      isValid(double val)
      {
        return Record< double >::DEFAULT_MIN() != val
            && Record< double >::DEFAULT_MAX() != val && !std::isnan(val)
            && !std::isinf(val);
      }

      template < typename Value >
      std::vector< MetricTankPublisherInterface::PublishData >
      recordToData(const TaggedRecords< Value > &taggedRecords, absl::Time time,
                   double elapsedTime, string_view suffix)
      {
        std::vector< MetricTankPublisherInterface::PublishData > result;

        std::string id = taggedRecords.id.toString();

        auto publicationType = taggedRecords.id.description()->type();

        for(const auto &record : taggedRecords.data)
        {
          const auto &tags = record.first;
          const auto &rec  = record.second;
          if(publicationType != Publication::Type::Unspecified)
          {
            auto val = formatValue(rec, elapsedTime, publicationType);

            if(val)
            {
              result.emplace_back(
                  addName(id, Publication::repr(publicationType), tags, suffix),
                  val.value(), time);
            }
          }
          else
          {
            result.emplace_back(addName(id, "count", tags, suffix),
                                std::to_string(rec.count()), time);
            result.emplace_back(addName(id, "total", tags, suffix),
                                std::to_string(rec.total()), time);

            if(isValid(rec.min()))
            {
              result.emplace_back(addName(id, "min", tags, suffix),
                                  std::to_string(rec.min()), time);
            }
            if(isValid(rec.max()))
            {
              result.emplace_back(addName(id, "max", tags, suffix),
                                  std::to_string(rec.max()), time);
            }
          }
        }
        return result;
      }

#ifndef _WIN32
      void
      publishData(const std::vector< std::string > &toSend,
                  const std::string &host, short port)
      {
        struct addrinfo hints;
        struct addrinfo *addrs;
        bzero(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        const std::string portAsStr = std::to_string(port);

        if(getaddrinfo(host.c_str(), portAsStr.c_str(), &hints, &addrs) != 0)
        {
          LogError("Failed to get address info");
          return;
        }

        int sock =
            ::socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);

        if(sock < 0)
        {
          LogError("Failed to open socket");
          freeaddrinfo(addrs);
          return;
        }

        if(connect(sock, addrs->ai_addr, addrs->ai_addrlen) < 0)
        {
          LogError("Failed to connect to metrictank");
          close(sock);
          freeaddrinfo(addrs);
          return;
        }

        freeaddrinfo(addrs);

        for(const std::string &val : toSend)
        {
          ssize_t sentLen = 0;

          do
          {
            sentLen =
                ::send(sock, val.c_str() + sentLen, val.size() - sentLen, 0);
            if(sentLen == -1)
            {
              LogError("Error ", strerror(errno));
            }
          } while((0 <= sentLen)
                  && (static_cast< size_t >(sentLen) < val.size()));
        }

        LogInfo("Sent ", toSend.size(), " metrics to metrictank");

        shutdown(sock, SHUT_RDWR);
        close(sock);
      }
#else
      void
      publishData(const std::vector< std::string > &toSend,
                  const std::string &host, short port)
      {
        struct addrinfo *addrs = NULL, hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        const std::string portAsStr = std::to_string(port);

        if(getaddrinfo(host.c_str(), portAsStr.c_str(), &hints, &addrs) != 0)
        {
          LogError("Failed to get address info");
          return;
        }

        SOCKET sock =
            ::socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);

        if(sock == INVALID_SOCKET)
        {
          LogError("Failed to open socket");
          freeaddrinfo(addrs);
          return;
        }

        if(connect(sock, addrs->ai_addr, addrs->ai_addrlen) == SOCKET_ERROR)
        {
          LogError("Failed to connect to metrictank");
          closesocket(sock);
          freeaddrinfo(addrs);
          return;
        }

        freeaddrinfo(addrs);

        for(const std::string &val : toSend)
        {
          int sentLen = 0;

          do
          {
            sentLen =
                ::send(sock, val.c_str() + sentLen, val.size() - sentLen, 0);
            if(sentLen == SOCKET_ERROR)
            {
              LogError("Error ", strerror(errno));
            }
          } while((0 <= sentLen)
                  && (static_cast< size_t >(sentLen) < val.size()));
        }

        shutdown(sock, SD_SEND);
        closesocket(sock);
      }
#endif

      MetricTankPublisherInterface::Tags
      updateTags(MetricTankPublisherInterface::Tags tags)
      {
        if(tags.count("system") == 0)
        {
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
          tags["system"] = "windows";
#elif defined(__APPLE__)
          tags["system"] = "macos";
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
          tags["system"] = "bsd";
#elif defined(__sun)
          tags["system"] = "solaris";
#elif defined(__linux__)
          tags["system"] = "linux";
#else
          tags["system"] = "unknown";
#endif
        }

        return tags;
      }
    }  // namespace

    std::string
    MetricTankPublisherInterface::makeSuffix(const Tags &tags)
    {
      return absl::StrJoin(updateTags(tags), ";", absl::PairFormatter("="));
    }

    void
    MetricTankPublisherInterface::publish(const Sample &values)
    {
      if(values.recordCount() == 0)
      {
        // nothing to publish
        return;
      }

      absl::Time sampleTime = values.sampleTime();

      std::vector< PublishData > result;
      result.reserve(values.recordCount());

      auto gIt  = values.begin();
      auto prev = values.begin();
      for(; gIt != values.end(); ++gIt)
      {
        const double elapsedTime = absl::ToDoubleSeconds(samplePeriod(*gIt));

        absl::visit(
            [&](const auto &d) {
              for(const auto &record : d)
              {
                auto partial =
                    recordToData(record, sampleTime, elapsedTime, m_suffix);
                result.insert(result.end(), partial.begin(), partial.end());
              }
            },
            *gIt);

        prev = gIt;
      }

      publish(result);
    }

    void
    MetricTankPublisher::publish(const std::vector< PublishData > &data)
    {
      if(m_queue.tryPushBack(data) == thread::QueueReturn::QueueFull)
      {
        LogWarn("Dropping metrictank logs!");
      }
    }

    void
    MetricTankPublisher::work()
    {
      while(true)
      {
        auto data = m_queue.popFront();  // block until we get something

        // Finish
        if(absl::holds_alternative< StopStruct >(data))
        {
          return;
        }

        assert(absl::holds_alternative< std::vector< PublishData > >(data));

        auto vec = absl::get< std::vector< PublishData > >(data);

        std::vector< std::string > toSend;
        toSend.reserve(vec.size());

        std::transform(vec.begin(), vec.end(), std::back_inserter(toSend),
                       [](const PublishData &d) -> std::string {
                         return absl::StrCat(
                             std::get< 0 >(d), " ", std::get< 1 >(d), " ",
                             absl::ToUnixSeconds(std::get< 2 >(d)), "\n");
                       });

        publishData(toSend, m_host, m_port);
      }
    }
  }  // namespace metrics
}  // namespace llarp

#include <metrics/metrictank_publisher.hpp>

#include <util/logger.hpp>

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
#include <lmcons.h>
#endif

namespace llarp
{
  namespace metrics
  {
    namespace
    {
      absl::optional< std::string >
      makeStr(double d)
      {
        if(std::isnan(d) || std::isinf(d))
        {
          return {};
        }
        else
        {
          return std::to_string(d);
        }
      }

      absl::optional< std::string >
      formatValue(const Record &record, double elapsedTime,
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
            return makeStr(record.total() / record.count());
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
      }

      std::string
      addName(string_view id, string_view name, string_view suffix)
      {
        return absl::StrCat(id, ".", name, suffix);
      }

      std::vector< MetricTankPublisherInterface::PublishData >
      recordToData(const Record &record, absl::Time time, double elapsedTime,
                   string_view suffix)
      {
        std::vector< MetricTankPublisherInterface::PublishData > result;

        std::string id = record.id().toString();

        auto publicationType = record.id().description()->type();
        if(publicationType != Publication::Type::Unspecified)
        {
          auto val = formatValue(record, elapsedTime, publicationType);

          if(val)
          {
            result.emplace_back(
                addName(id, Publication::repr(publicationType), suffix),
                val.value(), time);
          }
        }
        else
        {
          result.emplace_back(addName(id, "count", suffix),
                              std::to_string(record.count()), time);
          result.emplace_back(addName(id, "total", suffix),
                              std::to_string(record.total()), time);

          if(Record::DEFAULT_MIN != record.min() && !std::isnan(record.min())
             && !std::isinf(record.min()))
          {
            result.emplace_back(addName(id, "min", suffix),
                                std::to_string(record.min()), time);
          }
          if(Record::DEFAULT_MAX == record.max() && !std::isnan(record.max())
             && !std::isinf(record.max()))
          {
            result.emplace_back(addName(id, "max", suffix),
                                std::to_string(record.max()), time);
          }
        }
        return result;
      }

#ifndef _WIN32
      void
      publishData(const std::vector< std::string > &toSend,
                  const std::string &host, short port)
      {
        struct addrinfo hints, *addrs;
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
          ssize_t sentLen = 0;

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

        if(tags.count("user") == 0)
        {
#ifndef _WIN32
          const char *username = getlogin();
          if(username != nullptr)
          {
            tags["user"] = username;
          }
          else
          {
            tags["user"] = "unknown";
          }
#else
          char username[UNLEN + 1];
          DWORD username_len = UNLEN + 1;
          GetUserName(username, &username_len);
          tags["user"] = username;
#endif
        }

        return tags;
      }
    }  // namespace

    std::string
    MetricTankPublisherInterface::makeSuffix(const Tags &tags)
    {
      std::string result;
      for(const auto &tag : updateTags(tags))
      {
        absl::StrAppend(&result, ";", tag.first, "=", tag.second);
      }
      return result;
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
        const double elapsedTime = absl::ToDoubleSeconds(gIt->samplePeriod());

        for(const auto &record : *gIt)
        {
          auto partial =
              recordToData(record, sampleTime, elapsedTime, m_suffix);
          result.insert(result.end(), partial.begin(), partial.end());
        }
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

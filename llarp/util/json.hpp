#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <iostream>

namespace llarp
{
  namespace json
  {
    using Object = nlohmann::json;

    struct IParser
    {
      virtual ~IParser() = default;

      /// result from feeding data to parser
      enum Result
      {
        /// we need more data to finish parsing
        eNeedData,
        /// we have parsed the object fully
        eDone,
        /// we have a parsing syntax error
        eParseError
      };

      /// feed data to parser return true if successful
      virtual bool
      FeedData(const char* buf, size_t sz) = 0;
      /// parse internal buffer
      virtual Result
      Parse(Object& obj) const = 0;
    };

    /// create new parser
    IParser*
    MakeParser(size_t contentSize);

    /// maybe get a sub element in a dict by key or return a fallback
    template <typename T>
    std::optional<T>
    maybe_get(
        const nlohmann::json& obj, std::string_view data, std::optional<T> fallback = std::nullopt)
    {
      if (auto itr = obj.find(data); itr != obj.end())
        return itr->get<T>();
      return fallback;
    }
  }  // namespace json
}  // namespace llarp

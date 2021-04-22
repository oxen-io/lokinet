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

  }  // namespace json
}  // namespace llarp

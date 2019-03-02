#ifndef LLARP_UTIL_JSON_HPP
#define LLARP_UTIL_JSON_HPP

#include <util/string_view.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <iostream>

namespace llarp
{
  namespace json
  {
    struct IParser
    {
      virtual ~IParser(){};

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
      Parse(nlohmann::json& obj) const = 0;
    };

    /// create new parser
    IParser*
    MakeParser(size_t contentSize);

  }  // namespace json
}  // namespace llarp

#endif

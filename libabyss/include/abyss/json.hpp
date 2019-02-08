#ifndef __ABYSS_JSON_JSON_HPP
#define __ABYSS_JSON_JSON_HPP

#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/ostreamwrapper.h>
#include <iostream>

namespace abyss
{
  namespace json
  {
    using Document = rapidjson::Document;
    using Value    = rapidjson::Value;
    using Writer   = rapidjson::PrettyWriter< rapidjson::OStreamWrapper >;
  }  // namespace json

#if __cplusplus >= 201703L
  using string_view = std::string_view;
#else
  using string_view = std::string;
#endif
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
      FeedData(const char *buf, size_t sz) = 0;
      /// parse internal buffer
      virtual Result
      Parse(Document &obj) const = 0;
    };

    /// create new parser
    IParser *
    MakeParser(size_t contentSize);

    void
    ToString(const json::Document &obj, std::ostream &out);

  }  // namespace json
}  // namespace abyss

#endif

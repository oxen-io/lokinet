#ifndef __ABYSS_JSON_JSON_HPP
#define __ABYSS_JSON_JSON_HPP

#include <memory>
//#if __cplusplus >= 201703L
#if 0
#include <unordered_map>
#include <any>
namespace abyss
{
  namespace json
  {
    typedef std::unordered_map< std::string, std::any > Object;
  }
}  // namespace abyss
#else
#include <rapidjson/document.h>
namespace abyss
{
  namespace json
  {
    typedef rapidjson::Document Document;
    typedef rapidjson::Value Value;
  }  // namespace json
}  // namespace abyss
#endif
namespace abyss
{
// the code assumes C++11, if you _actually have_ C++17
// and have the ability to switch it on, then some of the code
// using string_view fails since it was made with the assumption
// that string_view is an alias for string!
//
// Specifically, one cannot use std::transform() on _real_ string_views
// since _those_ are read-only at all times - iterator and const_iterator
// are _exactly_ alike -rick

/* #if __cplusplus >= 201703L */
#if 0
  typedef std::string_view string_view;
#else
  typedef std::string string_view;
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
    ToString(const json::Document &obj, std::string &out);

  }  // namespace json
}  // namespace abyss

#endif

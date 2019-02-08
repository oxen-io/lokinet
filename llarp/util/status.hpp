#ifndef LLARP_UTIL_STATUS_HPP
#define LLARP_UTIL_STATUS_HPP
#ifdef USE_ABYSS
#include <abyss/json.hpp>
#endif
#include <vector>

namespace llarp
{
  namespace util
  {
#ifdef USE_ABYSS
    using StatusObject_Impl = ::abyss::json::Document;
    using Value_t           = ::abyss::json::Value;
#else
    struct StatusObject_Impl
    {
    };
    struct Value_t
    {
    }
#endif

    struct StatusObject
    {
      StatusObject();
      ~StatusObject();

      void
      PutInt(const char* name, uint64_t val);

      void
      PutString(const char* name, const std::string& val);

      void
      PutBool(const char* name, bool val);

      void
      PutObject(const char* name, StatusObject& obj);

      void
      PutStringArray(const char* name, std::vector< std::string >& arr);

      void
      PutObjectArray(const char* name, std::vector< StatusObject >& arr);

      StatusObject_Impl Impl;
    };

    /// an entity that has a status that can be extracted
    struct IStateful
    {
      virtual ~IStateful(){};

      virtual void
      ExtractStatus(StatusObject& state) const = 0;
    };

  }  // namespace util
}  // namespace llarp

#endif

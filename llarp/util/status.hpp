#ifndef LLARP_UTIL_STATUS_HPP
#define LLARP_UTIL_STATUS_HPP
#ifdef USE_ABYSS
#include <abyss/json.hpp>
#endif
#include <util/string_view.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <absl/types/variant.h>

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
    };
#endif

    struct StatusObject
    {
      using String_t = llarp::string_view;
      using Variant  = absl::variant< uint64_t, std::string, bool, StatusObject,
                                     std::vector< std::string >,
                                     std::vector< StatusObject > >;
      using value_type = std::tuple< String_t, Variant >;

      StatusObject(const StatusObject&);
      ~StatusObject();

      StatusObject(std::initializer_list< value_type > vals)
      {
#ifdef USE_ABYSS
        Impl.SetObject();
#endif
        std::for_each(vals.begin(), vals.end(),
                      [&](const value_type& item) { Put(item); });
      }

      void
      Put(String_t name, const Variant& value);

      void
      Put(const value_type& value);

      StatusObject_Impl Impl;

     private:
      void
      PutBool(String_t name, bool val);
      void
      PutInt(String_t name, uint64_t val);
      void
      PutObject(String_t name, const StatusObject& val);
      void
      PutObjectArray(String_t name, const std::vector< StatusObject >& arr);
      void
      PutStringArray(String_t name, const std::vector< std::string >& arr);
      void
      PutString(String_t name, const std::string& val);
    };

    /// an entity that has a status that can be extracted
    struct IStateful
    {
      virtual ~IStateful(){};

      virtual StatusObject
      ExtractStatus() const = 0;
    };

  }  // namespace util
}  // namespace llarp

#endif

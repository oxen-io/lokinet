#include <util/status.hpp>

namespace llarp
{
  namespace util
  {
    StatusObject::StatusObject(const StatusObject& other)
    {
#ifdef USE_ABYSS
      Impl.SetObject();
      auto& a = Impl.GetAllocator();
      Impl.CopyFrom(other.Impl, a);
#else
      (void)other;
#endif
    }

    StatusObject::~StatusObject()
    {
#ifdef USE_ABYSS
      Impl.RemoveAllMembers();
#endif
    }

    void
    StatusObject::Put(const value_type& val)
    {
      Put(std::get< 0 >(val), std::get< 1 >(val));
    }

    void
    StatusObject::Put(String_t name, const Variant& val)
    {
      if(absl::holds_alternative< uint64_t >(val))
        PutInt(name, absl::get< uint64_t >(val));
      else if(absl::holds_alternative< std::string >(val))
        PutString(name, absl::get< std::string >(val));
      else if(absl::holds_alternative< bool >(val))
        PutBool(name, absl::get< bool >(val));
      else if(absl::holds_alternative< StatusObject >(val))
        PutObject(name, absl::get< StatusObject >(val));
      else if(absl::holds_alternative< std::vector< std::string > >(val))
        PutStringArray(name, absl::get< std::vector< std::string > >(val));
      else if(absl::holds_alternative< std::vector< StatusObject > >(val))
        PutObjectArray(name, absl::get< std::vector< StatusObject > >(val));
    }

    void
    StatusObject::PutBool(String_t name, bool val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetBool(val);
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutInt(String_t name, uint64_t val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetUint64(val);
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObject(String_t name, const StatusObject& val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetObject();
      v.CopyFrom(val.Impl, a);
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObjectArray(String_t name,
                                 const std::vector< StatusObject >& arr)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetArray();
      Value_t i;
      for(const auto& obj : arr)
      {
        v.PushBack(i.SetObject().CopyFrom(obj.Impl, a), a);
      }
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)arr;
#endif
    }

    void
    StatusObject::PutStringArray(String_t name,
                                 const std::vector< std::string >& arr)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetArray();
      Value_t i;
      for(const auto& str : arr)
      {
        v.PushBack(i.SetString(str.c_str(), a), a);
      }
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)arr;
#endif
    }

    void
    StatusObject::PutString(String_t name, const std::string& val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetString(val.c_str(), a);
      auto s = llarp::string_view_string(name);
      Value_t k(s.c_str(), a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }
  }  // namespace util
}  // namespace llarp

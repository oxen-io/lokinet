#include <util/status.hpp>

namespace llarp
{
  namespace util
  {
    StatusObject::StatusObject()
    {
#ifdef USE_ABYSS
      Impl.SetObject();
#endif
    }

    StatusObject::~StatusObject()
    {
#ifdef USE_ABYSS
      Impl.RemoveAllMembers();
#endif
    }

    void
    StatusObject::PutBool(const char* name, bool val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetBool(val);
      Value_t k(name, a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutInt(const char* name, uint64_t val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetUint64(val);
      Value_t k(name, a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObject(const char* name, StatusObject& val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetObject();
      v.CopyFrom(val.Impl, a);
      Value_t k(name, a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObjectArray(const char* name,
                                 std::vector< StatusObject >& arr)
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
      Value_t k(name, a);

      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)arr;
#endif
    }

    void
    StatusObject::PutStringArray(const char* name,
                                 std::vector< std::string >& arr)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetArray();
      Value_t i;
      for(auto& str : arr)
      {
        v.PushBack(i.SetString(str.c_str(), a), a);
      }
      Value_t k(name, a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)arr;
#endif
    }

    void
    StatusObject::PutString(const char* name, const std::string& val)
    {
#ifdef USE_ABYSS
      auto& a = Impl.GetAllocator();
      Value_t v;
      v.SetString(val.c_str(), a);
      Value_t k(name, a);
      Impl.AddMember(k, v, a);
#else
      (void)name;
      (void)val;
#endif
    }
  }  // namespace util
}  // namespace llarp

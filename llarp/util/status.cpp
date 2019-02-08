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

    StatusObject::StatusObject(const StatusObject& other)
    {
#ifdef USE_ABYSS
      Impl.SetObject();
      Impl.CopyFrom(other.Impl, Impl.GetAllocator());
#else
      (void)other;
#endif
    }

    StatusObject::~StatusObject()
    {
    }

    void
    StatusObject::PutBool(const char* name, bool val)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetBool(val);
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutInt(const char* name, uint64_t val)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetInt(val);
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObject(const char* name, const StatusObject& val)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetObject();
      v.CopyFrom(val.Impl, Impl.GetAllocator());
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutObjectArray(const char* name,
                                 const std::vector< StatusObject >& arr)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetArray();
      for(const auto& obj : arr)
      {
        Value_t i;
        i.SetObject();
        i.CopyFrom(obj.Impl, Impl.GetAllocator());
        v.PushBack(i, Impl.GetAllocator());
      }
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutStringArray(const char* name,
                                 const std::vector< std::string >& arr)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetArray();
      for(const auto& str : arr)
      {
        Value_t i;
        i.SetString(str.c_str(), Impl.GetAllocator());
        v.PushBack(i, Impl.GetAllocator());
      }
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }

    void
    StatusObject::PutString(const char* name, const std::string& val)
    {
#ifdef USE_ABYSS
      Value_t v;
      v.SetString(val.c_str(), Impl.GetAllocator());
      Value_t k(name, Impl.GetAllocator());
      Impl.AddMember(k, v, Impl.GetAllocator());
#else
      (void)name;
      (void)val;
#endif
    }
  }  // namespace util
}  // namespace llarp

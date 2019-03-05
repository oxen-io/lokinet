#include <util/status.hpp>

#include <util/traits.hpp>

namespace llarp
{
  namespace util
  {
    struct StatusVisitor
    {
      std::string name;
      std::reference_wrapper< nlohmann::json > data;

      StatusVisitor(StatusObject::String_t n, nlohmann::json& d)
          : name(n), data(d)
      {
      }
      void
      operator()(uint64_t val)
      {
        data.get()[name] = val;
      }
      void
      operator()(const std::string& val)
      {
        data.get()[name] = val;
      }
      void
      operator()(bool val)
      {
        data.get()[name] = val;
      }
      void
      operator()(const StatusObject& obj)
      {
        data.get()[name] = obj.Impl;
      }
      void
      operator()(const std::vector< std::string >& val)
      {
        data.get()[name] = val;
      }
      void
      operator()(const std::vector< StatusObject >& val)
      {
        auto arr = nlohmann::json::array();
        std::transform(val.begin(), val.end(), std::back_inserter(arr),
                       [](const auto& x) { return x.Impl; });
        data.get()[name] = arr;
      }
    };
    void
    StatusObject::Put(const value_type& val)
    {
      Put(std::get< 0 >(val), std::get< 1 >(val));
    }

    void
    StatusObject::Put(String_t name, const Variant& data)
    {
      absl::visit(StatusVisitor{name, Impl}, data);
    }
  }  // namespace util
}  // namespace llarp

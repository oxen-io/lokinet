#ifndef LOKINET_BOOTSERV_CONFIG_HPP
#define LOKINET_BOOTSERV_CONFIG_HPP
#include <unordered_map>
#include <string_view>
#include <functional>
#include <memory>
#include <vector>

namespace lokinet
{
  namespace bootserv
  {
    struct Config
    {
      using String_t      = std::string_view;
      using Section_t     = std::unordered_map< String_t, String_t >;
      using Config_impl_t = std::unordered_map< String_t, Section_t >;

      static const char* DefaultPath;

      /// clear config
      void
      Clear();

      /// load config file for bootserv
      /// return true on success
      /// return false on error
      bool
      LoadFile(const char* fname);

      /// visit a section in config read only by name
      /// return false if no section or value propagated from visitor
      bool
      VisitSection(const char* name,
                   std::function< bool(const Section_t&) > visit) const;

     private:
      bool
      Parse();

      std::vector< char > m_Data;
      Config_impl_t m_Config;
    };
  }  // namespace bootserv
}  // namespace lokinet

#endif

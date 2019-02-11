#ifndef LLARP_CONFIG_HPP
#define LLARP_CONFIG_HPP

#include <forward_list>
#include <string>

namespace llarp
{
  struct Config
  {
    using section_t =
        std::forward_list< std::pair< std::string, std::string > >;

    section_t router;
    section_t network;
    section_t netdb;
    section_t dns;
    section_t iwp_links;
    section_t connect;
    section_t services;
    section_t system;
    section_t api;
    section_t lokid;
    section_t bootstrap;

    bool
    Load(const char *fname);

    using Visitor = std::function< void(const char *section, const char *key,
                                        const char *val) >;

    void
    visit(const Visitor &visitor);
  };

}  // namespace llarp

void
llarp_generic_ensure_config(std::ofstream &f, std::string basepath);

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath);

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath);

#endif

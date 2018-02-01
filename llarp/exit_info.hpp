#ifndef LLARP_XI_HPP
#define LLARP_XI_HPP
#include <llarp/exit_info.h>
#include <list>

struct llarp_xi_list {
  std::list<llarp_xi> list;
};

static std::list<llarp_xi> xi_list_to_std(struct llarp_xi_list *l) {
  std::list<llarp_xi> list;
  if (l->list.size())
    for (const auto &xi : l->list) list.push_back(xi);
  return list;
}

#endif

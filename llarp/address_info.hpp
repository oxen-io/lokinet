#ifndef LLARP_AI_HPP
#define LLARP_AI_HPP
#include <llarp/address_info.h>
#include <cstring>
#include <list>

struct llarp_ai_list
{
  llarp_ai * data;
  llarp_ai_list * next = nullptr;
};

static std::list<llarp_ai> ai_list_to_std(struct llarp_ai_list * l)
{
  std::list<llarp_ai> list;
  if(l && l->data)
  {
    do
    {
      llarp_ai copy;
      memcpy(&copy, l->data, sizeof(llarp_ai));
      list.push_back(copy);
      l = l->next;
    }
    while(l->next);
  }
  return list;
}

#endif


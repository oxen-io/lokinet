#ifndef LLARP_API_PARSER_HPP
#define LLARP_API_PARSER_HPP
#include <llarp/bencode.h>
#include <llarp/api/messages.hpp>

namespace llarp
{
  namespace api
  {
    struct MessageParser
    {
      MessageParser();

      IMessage *
      ParseMessage(llarp_buffer_t buf);

     private:
      static bool
      OnKey(dict_reader *r, llarp_buffer_t *key);
      IMessage *msg = nullptr;
      dict_reader r;
    };
  }  // namespace api
}  // namespace llarp

#endif
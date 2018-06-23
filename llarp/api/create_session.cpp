#include <list>
#include <llarp/api/messages.hpp>
#include <llarp/encrypted.hpp>
#include <string>

namespace llarp
{
  namespace api
  {

    bool
    CreateSessionMessage::DecodeParams(llarp_buffer_t *buf)
    {
      std::list< llarp::Encrypted > params;
      return BEncodeReadList(params, buf);
    }
  }  // namespace api
}  // namespace llarp
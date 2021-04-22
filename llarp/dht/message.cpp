#include "context.hpp"

#include <memory>
#include <llarp/util/bencode.hpp>
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/dht/messages/findname.hpp>
#include <llarp/dht/messages/gotname.hpp>

namespace llarp
{
  namespace dht
  {
    struct MessageDecoder
    {
      const Key_t& From;
      IMessage::Ptr_t msg;
      bool firstKey = true;
      bool relayed = false;

      MessageDecoder(const Key_t& from, bool wasRelayed) : From(from), relayed(wasRelayed)
      {}

      bool
      operator()(llarp_buffer_t* buffer, llarp_buffer_t* key)
      {
        llarp_buffer_t strbuf;
        // check for empty dict
        if (!key)
          return !firstKey;

        // first key
        if (firstKey)
        {
          if (!(*key == "A"))
            return false;
          if (!bencode_read_string(buffer, &strbuf))
            return false;
          // bad msg size?
          if (strbuf.sz != 1)
            return false;
          llarp::LogDebug("Handle DHT message ", *strbuf.base, " relayed=", relayed);
          switch (*strbuf.base)
          {
            case 'N':
              msg = std::make_unique<FindNameMessage>(From, Key_t{}, 0);
              break;
            case 'M':
              msg = std::make_unique<GotNameMessage>(From, 0, service::EncryptedName{});
              break;
            case 'F':
              msg = std::make_unique<FindIntroMessage>(From, relayed, 0);
              break;
            case 'R':
              if (relayed)
                msg = std::make_unique<RelayedFindRouterMessage>(From);
              else
                msg = std::make_unique<FindRouterMessage>(From);
              break;
            case 'S':
              msg = std::make_unique<GotRouterMessage>(From, relayed);
              break;
            case 'I':
              msg = std::make_unique<PublishIntroMessage>(From, relayed);
              break;
            case 'G':
              if (relayed)
              {
                msg = std::make_unique<RelayedGotIntroMessage>();
                break;
              }
              else
              {
                msg = std::make_unique<GotIntroMessage>(From);
                break;
              }
            default:
              llarp::LogWarn("unknown dht message type: ", (char)*strbuf.base);
              // bad msg type
              return false;
          }
          firstKey = false;
          return msg != nullptr;
        }

        return msg->DecodeKey(*key, buffer);
      }
    };

    IMessage::Ptr_t
    DecodeMesssage(const Key_t& from, llarp_buffer_t* buf, bool relayed)
    {
      MessageDecoder dec(from, relayed);
      if (!bencode_read_dict(dec, buf))
        return nullptr;

      return std::move(dec.msg);
    }

    struct ListDecoder
    {
      ListDecoder(bool hasRelayed, const Key_t& from, std::vector<IMessage::Ptr_t>& list)
          : relayed(hasRelayed), From(from), l(list)
      {}

      bool relayed;
      const Key_t& From;
      std::vector<IMessage::Ptr_t>& l;

      bool
      operator()(llarp_buffer_t* buffer, bool has)
      {
        if (!has)
          return true;
        auto msg = DecodeMesssage(From, buffer, relayed);
        if (msg)
        {
          l.emplace_back(std::move(msg));
          return true;
        }

        return false;
      }
    };

    bool
    DecodeMesssageList(
        Key_t from, llarp_buffer_t* buf, std::vector<IMessage::Ptr_t>& list, bool relayed)
    {
      ListDecoder dec(relayed, from, list);
      return bencode_read_list(dec, buf);
    }
  }  // namespace dht
}  // namespace llarp

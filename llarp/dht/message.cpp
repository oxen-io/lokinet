#include <dht/context.hpp>

#include <memory>
#include <util/bencode.hpp>
#include <dht/messages/findintro.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>

namespace llarp
{
  namespace dht
  {
    struct MessageDecoder
    {
      const Key_t &From;
      IMessage::Ptr_t msg;
      bool firstKey       = true;
      bool relayed        = false;
      uint32_t relayOrder = 0;

      MessageDecoder(const Key_t &from, bool wasRelayed, uint32_t order)
          : From(from), relayed(wasRelayed), relayOrder(order)
      {
      }

      bool
      operator()(llarp_buffer_t *buffer, llarp_buffer_t *key)
      {
        llarp_buffer_t strbuf;
        // check for empty dict
        if(!key)
          return !firstKey;

        // first key
        if(firstKey)
        {
          if(!(*key == "A"))
            return false;
          if(!bencode_read_string(buffer, &strbuf))
            return false;
          // bad msg size?
          if(strbuf.sz != 1)
            return false;
          llarp::LogDebug("Handle DHT message ", *strbuf.base,
                          " relayed=", relayed);
          switch(*strbuf.base)
          {
            case 'F':
              msg = std::make_unique< FindIntroMessage >(From, relayed,
                                                         relayOrder);
              break;
            case 'R':
              if(relayed)
                msg = std::make_unique< RelayedFindRouterMessage >(From);
              else
                msg = std::make_unique< FindRouterMessage >(From);
              break;
            case 'S':
              msg = std::make_unique< GotRouterMessage >(From, relayed);
              break;
            case 'I':
              msg = std::make_unique< PublishIntroMessage >();
              break;
            case 'G':
              if(relayed)
              {
                msg = std::make_unique< RelayedGotIntroMessage >();
                break;
              }
              else
              {
                msg = std::make_unique< GotIntroMessage >(From);
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
    DecodeMesssage(const Key_t &from, llarp_buffer_t *buf, bool relayed,
                   uint32_t relayOrder)
    {
      MessageDecoder dec(from, relayed, relayOrder);
      if(!bencode_read_dict(dec, buf))
        return nullptr;

      return std::move(dec.msg);
    }

    struct ListDecoder
    {
      ListDecoder(bool hasRelayed, uint32_t order, const Key_t &from,
                  std::vector< IMessage::Ptr_t > &list)
          : relayed(hasRelayed), relayOrder(order), From(from), l(list)
      {
      }

      bool relayed;
      uint32_t relayOrder;
      const Key_t &From;
      std::vector< IMessage::Ptr_t > &l;

      bool
      operator()(llarp_buffer_t *buffer, bool has)
      {
        if(!has)
          return true;
        auto msg = DecodeMesssage(From, buffer, relayed, relayOrder);
        if(msg)
        {
          l.emplace_back(std::move(msg));
          return true;
        }

        return false;
      }
    };

    bool
    DecodeMesssageList(Key_t from, llarp_buffer_t *buf,
                       std::vector< IMessage::Ptr_t > &list, bool relayed,
                       uint32_t relayOrder)
    {
      ListDecoder dec(relayed, relayOrder, from, list);
      return bencode_read_list(dec, buf);
    }
  }  // namespace dht
}  // namespace llarp

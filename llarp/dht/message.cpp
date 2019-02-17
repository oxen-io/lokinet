#include <dht/context.hpp>

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
      std::unique_ptr< IMessage > msg;
      bool firstKey = true;
      bool relayed  = false;

      MessageDecoder(const Key_t &from) : From(from)
      {
      }

      static bool
      on_key(dict_reader *r, llarp_buffer_t *key)
      {
        llarp_buffer_t strbuf;
        MessageDecoder *dec = static_cast< MessageDecoder * >(r->user);
        // check for empty dict
        if(!key)
          return !dec->firstKey;

        // first key
        if(dec->firstKey)
        {
          if(!(*key == "A"))
            return false;
          if(!bencode_read_string(r->buffer, &strbuf))
            return false;
          // bad msg size?
          if(strbuf.sz != 1)
            return false;
          llarp::LogInfo("Handle DHT message ", *strbuf.base,
                         " relayed=", dec->relayed);
          switch(*strbuf.base)
          {
            case 'F':
              dec->msg.reset(new FindIntroMessage(dec->From, dec->relayed));
              break;
            case 'R':
              if(dec->relayed)
                dec->msg.reset(new RelayedFindRouterMessage(dec->From));
              else
                dec->msg.reset(new FindRouterMessage(dec->From));
              break;
            case 'S':
              dec->msg.reset(new GotRouterMessage(dec->From, dec->relayed));
              break;
            case 'I':
              dec->msg.reset(new PublishIntroMessage());
              break;
            case 'G':
              if(dec->relayed)
              {
                dec->msg.reset(new RelayedGotIntroMessage());
                break;
              }
              else
              {
                dec->msg.reset(new GotIntroMessage(dec->From));
                break;
              }
            default:
              llarp::LogWarn("unknown dht message type: ", (char)*strbuf.base);
              // bad msg type
              return false;
          }
          dec->firstKey = false;
          return dec->msg != nullptr;
        }
        else
          return dec->msg->DecodeKey(*key, r->buffer);
      }
    };

    std::unique_ptr< IMessage >
    DecodeMesssage(const Key_t &from, llarp_buffer_t *buf, bool relayed)
    {
      MessageDecoder dec(from);
      dec.relayed = relayed;
      dict_reader r;
      r.user   = &dec;
      r.on_key = &MessageDecoder::on_key;
      if(!bencode_read_dict(buf, &r))
        return nullptr;

      return std::unique_ptr< IMessage >(std::move(dec.msg));
    }

    struct ListDecoder
    {
      ListDecoder(const Key_t &from,
                  std::vector< std::unique_ptr< IMessage > > &list)
          : From(from), l(list){};

      bool relayed = false;
      const Key_t &From;
      std::vector< std::unique_ptr< IMessage > > &l;

      static bool
      on_item(list_reader *r, bool has)
      {
        ListDecoder *dec = static_cast< ListDecoder * >(r->user);
        if(!has)
          return true;
        auto msg = DecodeMesssage(dec->From, r->buffer, dec->relayed);
        if(msg)
        {
          dec->l.emplace_back(std::move(msg));
          return true;
        }
        else
          return false;
      }
    };

    bool
    DecodeMesssageList(Key_t from, llarp_buffer_t *buf,
                       std::vector< std::unique_ptr< IMessage > > &list,
                       bool relayed)
    {
      ListDecoder dec(from, list);
      dec.relayed = relayed;
      list_reader r;
      r.user    = &dec;
      r.on_item = &ListDecoder::on_item;
      return bencode_read_list(buf, &r);
    }
  }  // namespace dht
}  // namespace llarp

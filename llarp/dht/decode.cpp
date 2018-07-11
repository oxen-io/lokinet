#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/all.hpp>

namespace llarp
{
  namespace dht
  {
    struct MessageDecoder
    {
      const Key_t &From;
      bool firstKey = true;
      IMessage *msg = nullptr;
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
          if(!llarp_buffer_eq(*key, "A"))
            return false;
          if(!bencode_read_string(r->buffer, &strbuf))
            return false;
          // bad msg size?
          if(strbuf.sz != 1)
            return false;
          switch(*strbuf.base)
          {
            case 'R':
              if(dec->relayed)
                dec->msg = new RelayedFindRouterMessage(dec->From);
              else
                dec->msg = new FindRouterMessage(dec->From);
              break;
            case 'S':
              if(dec->relayed)
              {
                llarp::LogWarn(
                    "GotRouterMessage found when parsing relayed DHT "
                    "message");
                return false;
              }
              else
                dec->msg = new GotRouterMessage(dec->From);
              break;
            case 'I':
              dec->msg = new PublishIntroMessage();
              break;
            case 'G':
              if(dec->relayed)
              {
                dec->msg = new GotIntroMessage();
                break;
              }
              else
              {
                llarp::LogWarn(
                    "GotIntroMessage found when parsing direct DHT message");
                return false;
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

    IMessage *
    DecodeMesssage(const Key_t &from, llarp_buffer_t *buf, bool relayed)
    {
      MessageDecoder dec(from);
      dec.relayed = relayed;
      dict_reader r;
      r.user   = &dec;
      r.on_key = &MessageDecoder::on_key;
      if(bencode_read_dict(buf, &r))
        return dec.msg;
      else
      {
        if(dec.msg)
          delete dec.msg;
        return nullptr;
      }
    }

    struct ListDecoder
    {
      ListDecoder(const Key_t &from, std::vector< IMessage * > &list)
          : From(from), l(list){};

      bool relayed = false;
      const Key_t &From;
      std::vector< IMessage * > &l;

      static bool
      on_item(list_reader *r, bool has)
      {
        ListDecoder *dec = static_cast< ListDecoder * >(r->user);
        if(!has)
          return true;
        auto msg = DecodeMesssage(dec->From, r->buffer, dec->relayed);
        if(msg)
        {
          dec->l.push_back(msg);
          return true;
        }
        else
          return false;
      }
    };

    bool
    DecodeMesssageList(const Key_t &from, llarp_buffer_t *buf,
                       std::vector< IMessage * > &list, bool relayed)
    {
      ListDecoder dec(from, list);
      dec.relayed = relayed;
      list_reader r;
      r.user    = &dec;
      r.on_item = &ListDecoder::on_item;
      return bencode_read_list(buf, &r);
    }
  }
}
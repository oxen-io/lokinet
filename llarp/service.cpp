#include <llarp/service.hpp>

namespace llarp
{
  namespace service
  {
    ServiceInfo::~ServiceInfo()
    {
    }

    bool
    ServiceInfo::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("e", enckey, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("s", signkey, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("x", vanity, read, key, val))
        return false;
      return read;
    }

    bool
    ServiceInfo::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("e", enckey, buf))
        return false;
      if(!BEncodeWriteDictEntry("s", signkey, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "v", LLARP_PROTO_VERSION))
        return false;
      if(!vanity.IsZero())
      {
        if(!BEncodeWriteDictEntry("x", vanity, buf))
          return false;
      }
      return bencode_end(buf);
    }

    IntroSet::~IntroSet()
    {
    }

    bool
    IntroSet::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      // TODO: implement me
      return false;
    }

    bool
    IntroSetDecodeKey(dict_reader* r, llarp_buffer_t* key)
    {
      IntroSet* self = static_cast< IntroSet* >(r->user);
      if(!key)
        // TODO: determine if we read anything
        return true;
      return self->DecodeKey(*key, r->buffer);
    }

    bool
    IntroSet::BDecode(llarp_buffer_t* buf)
    {
      dict_reader r;
      r.user   = this;
      r.on_key = &IntroSetDecodeKey;
      return bencode_read_dict(buf, &r);
    }

    bool
    IntroSet::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      return bencode_end(buf);
    }

  }  // namespace service
}  // namespace llarp

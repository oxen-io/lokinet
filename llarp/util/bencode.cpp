#include <util/bencode.h>

#include <util/logger.hpp>

#include <cstdlib>
#include <inttypes.h>

bool
bencode_read_integer(struct llarp_buffer_t* buffer, uint64_t* result)
{
  size_t len;
  if(*buffer->cur != 'i')
    return false;

  char numbuf[32];

  buffer->cur++;

  len = buffer->read_until('e', (byte_t*)numbuf, sizeof(numbuf) - 1);
  if(!len)
  {
    return false;
  }

  buffer->cur++;

  numbuf[len] = '\0';
  *result     = std::strtoull(numbuf, nullptr, 10);
  return true;
}

bool
bencode_read_string(llarp_buffer_t* buffer, llarp_buffer_t* result)
{
  char numbuf[10];

  size_t len = buffer->read_until(':', (byte_t*)numbuf, sizeof(numbuf) - 1);
  if(!len)
    return false;

  numbuf[len]   = '\0';
  const int num = atoi(numbuf);
  if(num < 0)
  {
    return false;
  }

  const size_t slen = num;

  buffer->cur++;

  len = buffer->size_left();
  if(len < slen)
  {
    return false;
  }

  result->base = buffer->cur;
  result->cur  = buffer->cur;
  result->sz   = slen;
  buffer->cur += slen;
  return true;
}

bool
bencode_write_bytestring(llarp_buffer_t* buff, const void* data, size_t sz)
{
  if(!buff->writef("%zu:", sz))
  {
    return false;
  }
  return buff->write(reinterpret_cast< const char* >(data),
                     reinterpret_cast< const char* >(data) + sz);
}

bool
bencode_write_uint64(llarp_buffer_t* buff, uint64_t i)
{
  if(!buff->writef("i%" PRIu64, i))
  {
    return false;
  }

  static const char letter[1] = {'e'};
  assert(std::distance(std::begin(letter), std::end(letter)) == 1);
  return buff->write(std::begin(letter), std::end(letter));
}

bool
bencode_write_version_entry(llarp_buffer_t* buff)
{
  return buff->writef("1:vi%de", LLARP_PROTO_VERSION);
}

bool
bencode_start_list(llarp_buffer_t* buff)
{
  static const char letter[1] = {'l'};
  assert(std::distance(std::begin(letter), std::end(letter)) == 1);
  return buff->write(std::begin(letter), std::end(letter));
}

bool
bencode_start_dict(llarp_buffer_t* buff)
{
  static const char letter[1] = {'d'};
  assert(std::distance(std::begin(letter), std::end(letter)) == 1);
  return buff->write(std::begin(letter), std::end(letter));
}

bool
bencode_end(llarp_buffer_t* buff)
{
  static const char letter[1] = {'e'};
  assert(std::distance(std::begin(letter), std::end(letter)) == 1);
  return buff->write(std::begin(letter), std::end(letter));
}

bool
bencode_read_dict(llarp_buffer_t* buff, struct dict_reader* r)
{
  llarp_buffer_t strbuf;      // temporary buffer for current element
  r->buffer = buff;           // set up dict_reader
  if(*r->buffer->cur != 'd')  // ensure is a dictionary
    return false;
  r->buffer->cur++;
  while(r->buffer->size_left() && *r->buffer->cur != 'e')
  {
    if(bencode_read_string(r->buffer, &strbuf))
    {
      if(!r->on_key(r, &strbuf))  // check for early abort
        return false;
    }
    else
      return false;
  }

  if(*r->buffer->cur != 'e')
  {
    llarp::LogWarn("reading dict not ending on 'e'");
    // make sure we're at dictionary end
    return false;
  }
  r->buffer->cur++;
  return r->on_key(r, nullptr);
}

bool
bencode_read_list(llarp_buffer_t* buff, struct list_reader* r)
{
  r->buffer = buff;
  if(*r->buffer->cur != 'l')  // ensure is a list
  {
    llarp::LogWarn("bencode::bencode_read_list - expecting list got ",
                   *r->buffer->cur);
    return false;
  }

  r->buffer->cur++;
  while(r->buffer->size_left() && *r->buffer->cur != 'e')
  {
    if(!r->on_item(r, true))  // check for early abort
      return false;
  }
  if(*r->buffer->cur != 'e')  // make sure we're at a list end
    return false;
  r->buffer->cur++;
  return r->on_item(r, false);
}

#include <llarp/address_info.h>
#include <stdio.h>
#include <string.h>

bool llarp_address_info_bencode(struct llarp_address_info * ai, llarp_buffer_t * buff)
{
  char * ptr = buff->cur;
  size_t sz = llarp_buffer_size_left(buff);
  char * end = ptr + sz;
  int r = 0;
  r = snprintf(ptr, (end - ptr), "d1:ci%de1:e32:", ai->rank);
  if (r == -1) return false;
  ptr += r;
  if( ( end - ptr ) <= 0 ) return false;
  
  memcpy(ptr, ai->enc_key, sizeof(ai->enc_key));
  ptr += sizeof(ai->enc_key);
  if( ( end - ptr ) <= 0 ) return false;
  
  r = snprintf(ptr, (end - ptr), "1:d%ld:%s", strlen(ai->dialect), ai->dialect);
  if (r == -1) return false;
  ptr += r;
  if( ( end - ptr ) <= 0 ) return false;

  r = snprintf(ptr, (end - ptr), "1:i%ld:", sizeof(ai->ip));
  if(r == -1) return false;
  ptr += r;
  if( ( end - ptr ) <= 0) return false;
    
  memcpy(ptr, &ai->ip, sizeof(ai->ip));
  ptr += sizeof(ai->ip);
  if( ( end - ptr ) <= 0) return false;

  r = snprintf(ptr, (end - ptr), "1:pi%dee", ai->port);
  if( r == -1) return false;
  ptr += r;
  buff->cur = ptr;
  return end - ptr <= sz;
}

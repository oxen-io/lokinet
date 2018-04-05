#include <llarp/exit_route.h>
#include <llarp/bencode.h>
#include <llarp/string.h>
#include <arpa/inet.h>

bool llarp_xr_bencode(struct llarp_xr * xr, llarp_buffer_t * buff)
{

  char addr_buff [128] = {0};
  const char * addr;
  
  if(!bencode_start_dict(buff)) return false;

  /** gateway */
  addr = inet_ntop(AF_INET6, &xr->gateway, addr_buff, sizeof(addr_buff));
  if(!addr) return false;
  if(!bencode_write_bytestring(buff, "a", 1)) return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff)))) return false;
  
  /** netmask */
  addr = inet_ntop(AF_INET6, &xr->netmask, addr_buff, sizeof(addr_buff));
  if(!addr) return false;
  if(!bencode_write_bytestring(buff, "b", 1)) return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff)))) return false;
  
   /** source */
  addr = inet_ntop(AF_INET6, &xr->source, addr_buff, sizeof(addr_buff));
  if(!addr) return false;
  if(!bencode_write_bytestring(buff, "c", 1)) return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff)))) return false;

  /** lifetime */
  if(!bencode_write_bytestring(buff, "l", 1)) return false;
  if(!bencode_write_uint64(buff, xr->lifetime)) return false;

  /** version */
  if(!bencode_write_version_entry(buff)) return false;

  /* end */
  return bencode_end(buff);
}

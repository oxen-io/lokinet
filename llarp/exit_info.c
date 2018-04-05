#include <llarp/exit_info.h>
#include <llarp/bencode.h>
#include <llarp/string.h>
#include <arpa/inet.h>


bool llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buff) {
  char addr_buff[128] = {0};
  const char * addr;
  if(!bencode_start_dict(buff)) return false;

  /** address */
  addr = inet_ntop(AF_INET6, &xi->address, addr_buff, sizeof(addr_buff));
  if(!addr) return false;
  if(!bencode_write_bytestring(buff, "a", 1)) return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff)))) return false;

  /** netmask */
  addr = inet_ntop(AF_INET6, &xi->netmask, addr_buff, sizeof(addr_buff));
  if(!addr) return false;
  if(!bencode_write_bytestring(buff, "b", 1)) return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff)))) return false;

  /** public key */
  if(!bencode_write_bytestring(buff, "k", 1)) return false;
  if(!bencode_write_bytestring(buff, xi->pubkey, sizeof(llarp_pubkey_t))) return false;

  /** version */
  if(!bencode_write_version_entry(buff)) return false;
  
  return bencode_end(buff);
}


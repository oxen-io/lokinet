#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <sys/socket.h>
#include <string>

namespace llarp {
namespace net {
bool GetIfAddr(const std::string& ifname, int af, sockaddr* addr);
}
}  // namespace llarp

#endif

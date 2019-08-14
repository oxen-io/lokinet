#include <net/net_inaddr.hpp>

std::ostream&
operator<<(std::ostream& out, const llarp::inAddr& a)
{
  char tmp[128] = {0};
  if(a.isIPv6Mode())
  {
    out << "[";
  }
  if(inet_ntop(a.isIPv4Mode() ? AF_INET : AF_INET6, (void*)&a._addr, tmp,
               sizeof(tmp)))
  {
    out << tmp;
    if(a.isIPv6Mode())
      out << "]";
  }
  return out;
}

namespace llarp
{
  void
  inAddr::reset()
  {
    llarp::Zero(&this->_addr, sizeof(in6_addr));
  }

  bool
  inAddr::from_char_array(const char* str)
  {
    this->reset();

    // maybe refactor the family detection out
    struct addrinfo hint, *res = nullptr;
    int ret;

    memset(&hint, '\0', sizeof hint);

    hint.ai_family = PF_UNSPEC;
    hint.ai_flags  = AI_NUMERICHOST;

    ret = getaddrinfo(str, nullptr, &hint, &res);
    if(ret)
    {
      llarp::LogError("failed to determine address family: ", str);
      return false;
    }

    if(res->ai_family != AF_INET && res->ai_family != AF_INET6)
    {
      llarp::LogError("Address family not supported yet", str);
      return false;
    }

    // convert detected-family (ipv4 or ipv6) str to in6_addr

    /*
    if (res->ai_family == AF_INET)
    {
      freeaddrinfo(res);
      // get IPv4
      struct in_addr addr; // basically a uint32_t network order
      if(inet_aton(str, &addr) == 0)
      {
        llarp::LogError("failed to parse ", str);
        return false;
      }
      nuint32_t result;
      result.n = addr.s_addr;
      this->fromN32(result);
      return true;
    }
    */

    ret = inet_pton(res->ai_family, str, &this->_addr);
    // inet_pton won't set SIIT
    // this->hexDebug();
    freeaddrinfo(res);
    if(ret <= 0)
    {
      if(ret == 0)
      {
        llarp::LogWarn("Not in presentation format");
        return false;
      }

      llarp::LogWarn("inet_pton failure");
      return false;
    }
    return true;
  }

  void
  inAddr::fromSIIT()
  {
    if(ipv6_is_siit(this->_addr))
    {
      this->_addr.s6_addr[0] = this->_addr.s6_addr[12];
      this->_addr.s6_addr[1] = this->_addr.s6_addr[13];
      this->_addr.s6_addr[2] = this->_addr.s6_addr[14];
      this->_addr.s6_addr[3] = this->_addr.s6_addr[15];
      this->setIPv4Mode();
    }
  }

  void
  inAddr::toSIIT()
  {
    if(!ipv6_is_siit(this->_addr))
    {
      this->_addr.s6_addr[10] = 0xff;
      this->_addr.s6_addr[11] = 0xff;
      this->_addr.s6_addr[12] = this->_addr.s6_addr[0];
      this->_addr.s6_addr[13] = this->_addr.s6_addr[1];
      this->_addr.s6_addr[14] = this->_addr.s6_addr[2];
      this->_addr.s6_addr[15] = this->_addr.s6_addr[3];
      llarp::Zero(&this->_addr, sizeof(in6_addr) - 6);
    }
  }

  inline bool
  inAddr::isIPv6Mode() const
  {
    return !this->isIPv4Mode();
  }

  bool
  inAddr::isIPv4Mode() const
  {
    return ipv6_is_siit(this->_addr)
        || (this->_addr.s6_addr[4] == 0 && this->_addr.s6_addr[5] == 0
            && this->_addr.s6_addr[6] == 0 && this->_addr.s6_addr[7] == 0
            && this->_addr.s6_addr[8] == 0 && this->_addr.s6_addr[9] == 0
            && this->_addr.s6_addr[10] == 0 && this->_addr.s6_addr[11] == 0
            && this->_addr.s6_addr[12] == 0 && this->_addr.s6_addr[13] == 0
            && this->_addr.s6_addr[14] == 0 && this->_addr.s6_addr[15] == 0);
  }

  void
  inAddr::setIPv4Mode()
  {
    // keep first 4
    // llarp::Zero(&this->_addr + 4, sizeof(in6_addr) - 4);
    this->_addr.s6_addr[4]  = 0;
    this->_addr.s6_addr[5]  = 0;
    this->_addr.s6_addr[6]  = 0;
    this->_addr.s6_addr[7]  = 0;
    this->_addr.s6_addr[8]  = 0;
    this->_addr.s6_addr[9]  = 0;
    this->_addr.s6_addr[10] = 0;
    this->_addr.s6_addr[11] = 0;
    this->_addr.s6_addr[12] = 0;
    this->_addr.s6_addr[13] = 0;
    this->_addr.s6_addr[14] = 0;
    this->_addr.s6_addr[15] = 0;
  }

  void
  inAddr::hexDebug()
  {
    char hex_buffer[16 * 3 + 1];
    hex_buffer[16 * 3] = 0;
    for(unsigned int j = 0; j < 16; j++)
      sprintf(&hex_buffer[3 * j], "%02X ", this->_addr.s6_addr[j]);
    printf("in6_addr: [%s]\n", hex_buffer);
  }

  //
  // IPv4 specific functions
  //

  in_addr
  inAddr::toIAddr()
  {
    in_addr res;
    res.s_addr = toN32().n;
    return res;
  }

  void
  inAddr::from4int(const uint8_t one, const uint8_t two, const uint8_t three,
                   const uint8_t four)
  {
    this->reset();
    this->setIPv4Mode();
    // Network byte order
    this->_addr.s6_addr[0] = one;
    this->_addr.s6_addr[1] = two;
    this->_addr.s6_addr[2] = three;
    this->_addr.s6_addr[3] = four;
  }

  void
  inAddr::fromN32(nuint32_t in)
  {
    this->reset();
    this->setIPv4Mode();
    memcpy(&this->_addr, &in.n, sizeof(uint32_t));
  }
  void
  inAddr::fromH32(huint32_t in)
  {
    this->fromN32(xhtonl(in));
  }

  nuint32_t
  inAddr::toN32()
  {
    nuint32_t result;
    result.n = 0;  // return 0 for IPv6
    if(this->isIPv4Mode())
    {
      memcpy(&result.n, &this->_addr, sizeof(uint32_t));
    }
    return result;
  }
  huint32_t
  inAddr::toH32()
  {
    return xntohl(this->toN32());
  }

  //
  // IPv6 specific functions
  //

}  // namespace llarp

#include <llarp/dns/name.hpp>
#include <llarp/net.hpp>
#include <sstream>
#include <algorithm>

namespace llarp
{
  namespace dns
  {
    bool
    DecodeName(llarp_buffer_t* buf, Name_t& name)
    {
      if(llarp_buffer_size_left(*buf) < 1)
        return false;
      std::stringstream ss;
      size_t l;
      do
      {
        l = *buf->cur;
        buf->cur++;
        if(llarp_buffer_size_left(*buf) < l)
          return false;
        if(l)
          ss << Name_t((const char*)buf->cur, l);
        ss << ".";
      } while(l);
      name = ss.str();
      return true;
    }

    bool
    EncodeName(llarp_buffer_t* buf, const Name_t& name)
    {
      std::stringstream ss(name);
      if(name.size() == 0 || name[name.size() - 1] != '.')
        ss << ".";
      std::string part;
      while(std::getline(ss, part, '.'))
      {
        uint8_t l;
        if(part.size() > 63)
          return false;
        l         = part.size();
        *buf->cur = l;
        buf->cur++;
        if(llarp_buffer_size_left(*buf) < l)
          return false;
        memcpy(buf->cur, part.c_str(), l);
      }
      return true;
    }

    bool
    DecodePTR(const Name_t& name, huint32_t& ip)
    {
      auto pos = name.find(".in-addr.arpa.");
      if(pos == Name_t::npos)
        return false;
      std::string sub = name.substr(0, pos - 1);
      if(std::count(sub.begin(), sub.end(), '.') == 4)
      {
        uint8_t a, b, c, d;
        pos = sub.find('.');
        d   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos);
        pos = sub.find('.');
        c   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos);
        pos = sub.find('.');
        b   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos);
        pos = sub.find('.');
        a   = atoi(sub.substr(0, pos).c_str());
        ip  = llarp::ipaddr_ipv4_bits(a, b, c, d);
        return true;
      }
      else
        return false;
    }

  }  // namespace dns
}  // namespace llarp

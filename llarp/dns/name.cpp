#define __USE_MINGW_ANSI_STDIO 1
#include <dns/name.hpp>
#include <net/net.hpp>
#include <net/ip.hpp>

#include <algorithm>
#include <sstream>

namespace llarp
{
  namespace dns
  {
    bool
    DecodeName(llarp_buffer_t* buf, Name_t& name, bool trimTrailingDot)
    {
      if(buf->size_left() < 1)
        return false;
      std::stringstream ss;
      size_t l;
      do
      {
        l = *buf->cur;
        buf->cur++;
        if(l)
        {
          if(buf->size_left() < l)
            return false;

          ss << Name_t((const char*)buf->cur, l);
          ss << ".";
        }
        buf->cur = buf->cur + l;
      } while(l);
      name = ss.str();
      /// trim off last dot
      if(trimTrailingDot)
        name = name.substr(0, name.find_last_of('.'));
      return true;
    }

    bool
    EncodeName(llarp_buffer_t* buf, const Name_t& name)
    {
      std::stringstream ss;
      if(name.size() && name[name.size() - 1] == '.')
        ss << name.substr(0, name.size() - 1);
      else
        ss << name;

      std::string part;
      while(std::getline(ss, part, '.'))
      {
        size_t l = part.length();
        if(l > 63)
          return false;
        *(buf->cur) = l;
        buf->cur++;
        if(buf->size_left() < l)
          return false;
        if(l)
        {
          memcpy(buf->cur, part.data(), l);
          buf->cur += l;
        }
        else
          break;
      }
      *buf->cur = 0;
      buf->cur++;
      return true;
    }

    bool
    DecodePTR(const Name_t& name, huint128_t& ip)
    {
      bool isV6 = false;
      auto pos  = name.find(".in-addr.arpa");
      if(pos == std::string::npos)
      {
        pos  = name.find(".ip6.arpa");
        isV6 = true;
      }
      if(pos == std::string::npos)
        return false;
      std::string sub    = name.substr(0, pos + 1);
      const auto numdots = std::count(sub.begin(), sub.end(), '.');
      if(numdots == 4 && !isV6)
      {
        uint8_t a, b, c, d;
        pos = sub.find('.');
        d   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos + 1);
        pos = sub.find('.');
        c   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos + 1);
        pos = sub.find('.');
        b   = atoi(sub.substr(0, pos).c_str());
        sub = sub.substr(pos + 1);
        pos = sub.find('.');
        a   = atoi(sub.substr(0, pos).c_str());
        ip  = net::IPPacket::ExpandV4(llarp::ipaddr_ipv4_bits(a, b, c, d));
        return true;
      }
      if(numdots == 32 && isV6)
      {
        size_t idx = 0;
        uint8_t lo, hi;
        auto* ptr = (uint8_t*)&ip.h;
        while(idx < 16)
        {
          pos      = sub.find('.');
          lo       = (*sub.substr(0, pos).c_str()) - 'a';
          sub      = sub.substr(pos + 1);
          pos      = sub.find('.');
          hi       = (*sub.substr(0, pos).c_str()) - 'a';
          sub      = sub.substr(pos + 1);
          ptr[idx] = lo | (hi << 4);
          ++idx;
        }
        return true;
      }

      return false;
    }

  }  // namespace dns
}  // namespace llarp

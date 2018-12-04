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
        if(l)
        {
          if(l > 63)
          {
            llarp::LogError("decode name failed, field too big: ", l, " > 63");
            llarp::DumpBuffer(*buf);
            return false;
          }
          if(llarp_buffer_size_left(*buf) < l)
            return false;

          ss << Name_t((const char*)buf->cur, l);
          ss << ".";
        }
        buf->cur = buf->cur + l;
      } while(l);
      name = ss.str();
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
        if(llarp_buffer_size_left(*buf) < l)
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
    DecodePTR(const Name_t& name, huint32_t& ip)
    {
      auto pos = name.find(".in-addr.arpa");
      if(pos == std::string::npos)
        return false;
      std::string sub = name.substr(0, pos + 1);
      if(std::count(sub.begin(), sub.end(), '.') == 4)
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
        ip  = llarp::ipaddr_ipv4_bits(a, b, c, d);
        return true;
      }
      else
        return false;
    }

  }  // namespace dns
}  // namespace llarp

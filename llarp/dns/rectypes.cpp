#include <dns.hpp>  // for vput16bits()
#include <dns/rectypes.hpp>

namespace llarp
{
  namespace dns
  {
    record::~record() = default;

    bool
    record::parse(std::vector< byte_t > bytes)
    {
      return bytes.size() ? true : false;
    }

    std::vector< byte_t >
    record::to_bytes()
    {
      std::vector< byte_t > retval;
      return retval;
    }

    type_1a::type_1a() : record()
    {
      this->ipaddr.h = 0;
    }

    bool
    type_1a::parse(std::vector< byte_t > bytes)
    {
      if(bytes.size() < 4)
      {
        LogWarn("Less than 4 bytes passed in");
        return false;
      }
      // endian problems? no, it should come in, in network order
      /*
      LogDebug("Read ", std::to_string(bytes[0]), ".",
              std::to_string(bytes[1]), ".",
              std::to_string(bytes[2]), ".",
              std::to_string(bytes[3]));
      */
      this->ipaddr = ipaddr_ipv4_bits(bytes[3], bytes[2], bytes[1], bytes[0]);
      // LogDebug("Test ", this->ipaddr);
      return bytes.size() ? true : false;
    }

    std::vector< byte_t >
    type_1a::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(retval, 4);               // rdLength
      vput32bits(retval, this->ipaddr.h);  // write IP
      return retval;
    }

    type_2ns::type_2ns() : record()
    {
    }

    bool
    type_2ns::parse(std::vector< byte_t > bytes)
    {
      // trim last 2 bytes... probably the size
      this->ns = std::string(reinterpret_cast< char* >(bytes.data()),
                             bytes.size() - 2);
      return true;
    }

    std::vector< byte_t >
    type_2ns::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(retval, 2 + this->ns.length());  // rdLength
      vcode_domain(retval, this->ns);
      return retval;
    }

    type_5cname::type_5cname() : record()
    {
    }

    bool
    type_5cname::parse(std::vector< byte_t > bytes)
    {
      // trim last 2 bytes... probably the size
      this->cname =
          std::string(reinterpret_cast< char* >(bytes.data()), bytes.size());
      // LogDebug("type5 parsed ", this->cname);
      return true;
    }

    std::vector< byte_t >
    type_5cname::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(retval, 2 + this->cname.length());  // rdLength
      vcode_domain(retval, this->cname);
      return retval;
    }

    type_6soa::type_6soa() : record()
    {
      this->serial  = 0;
      this->refresh = 0;
      this->retry   = 0;
      this->expire  = 0;
      this->minimum = 0;
    }

    bool
    type_6soa::parse(std::vector< byte_t > bytes)
    {
      // FIXME: implmement me
      // this->cname = std::string(reinterpret_cast<char *>(bytes.data()),
      // bytes.size());
      return bytes.size() ? true : false;
    }

    std::vector< byte_t >
    type_6soa::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(
          retval,
          4 + this->mname.length() + this->rname.length() + 20);  // rdLength
      vcode_domain(retval, this->mname);
      vcode_domain(retval, this->rname);
      vput32bits(retval, this->serial);
      vput32bits(retval, this->refresh);
      vput32bits(retval, this->retry);
      vput32bits(retval, this->expire);
      vput32bits(retval, this->minimum);

      return retval;
    }

    type_12ptr::type_12ptr() : record()
    {
    }

    bool
    type_12ptr::parse(std::vector< byte_t > bytes)
    {
      this->revname =
          std::string(reinterpret_cast< char* >(bytes.data()), bytes.size());
      return bytes.size() ? true : false;
    }

    std::vector< byte_t >
    type_12ptr::to_bytes()
    {
      std::vector< byte_t > retval;
      // revname has 2 extra bytes at the end we don't want or need
      vput16bits(retval, 2 + this->revname.length());  // rdLength
      vcode_domain(retval, this->revname);
      // vput16bits(retval, this->revname.length());  // rdLength
      // vcode_domain(retval, this->revname.substr(0, this->revname.size() -
      // 2));
      return retval;
    }

    type_15mx::type_15mx() : record()
    {
      this->priority = 99;
    }

    bool
    type_15mx::parse(std::vector< byte_t > bytes)
    {
      this->mx =
          std::string(reinterpret_cast< char* >(bytes.data()), bytes.size());
      // LogInfo("parsed ", this->mx);
      return true;
    }

    std::vector< byte_t >
    type_15mx::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(retval, 2 + (2 + this->mx.length()));  // rdLength
      vput16bits(retval, this->priority);               // priority
      vcode_domain(retval, this->mx);
      return retval;
    }

    type_16txt::type_16txt() : record()
    {
    }

    bool
    type_16txt::parse(std::vector< byte_t > bytes)
    {
      this->txt = std::string(reinterpret_cast< char* >(bytes.data()),
                              bytes.size() - 1);
      return true;
    }

    std::vector< byte_t >
    type_16txt::to_bytes()
    {
      std::vector< byte_t > retval;
      vput16bits(retval, 1 + this->txt.length());  // rdLength
      retval.push_back(this->txt.length());        // length
      for(auto it : this->txt)
      {
        retval.push_back(it);
      }
      return retval;
    }

  }  // namespace dns
}  // namespace llarp

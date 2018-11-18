#ifndef LIBLLARP_DNS_REC_TYPES_HPP
#define LIBLLARP_DNS_REC_TYPES_HPP

#include <vector>
#include <llarp/buffer.h>  // for byte_t
#include <llarp/net.hpp>   // for llarp::Addr , llarp::huint32_t

namespace llarp
{
  namespace dns
  {
    struct record
    {
      virtual ~record(){};
      record()
      {
      }

      virtual bool
      parse(std::vector< byte_t > bytes);

      virtual std::vector< byte_t >
      to_bytes();
    };

    struct type_1a : public record
    {
      huint32_t ipaddr;

      virtual ~type_1a(){};
      type_1a();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

    struct type_2ns : public record
    {
      std::string ns;

      virtual ~type_2ns(){};
      type_2ns();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

    struct type_5cname : public record
    {
      std::string cname;

      virtual ~type_5cname(){};
      type_5cname();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

    struct type_12ptr : public record
    {
      std::string revname;

      virtual ~type_12ptr(){};
      type_12ptr();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

    struct type_15mx : public record
    {
      std::string mx;
      uint16_t priority;

      virtual ~type_15mx(){};
      type_15mx();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

    struct type_16txt : public record
    {
      std::string txt;

      virtual ~type_16txt(){};
      type_16txt();

      bool
      parse(std::vector< byte_t > bytes);

      std::vector< byte_t >
      to_bytes();
    };

  }  // namespace dns
}  // namespace llarp

#endif

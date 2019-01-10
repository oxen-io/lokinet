#ifndef LIBLLARP_DNS_REC_TYPES_HPP
#define LIBLLARP_DNS_REC_TYPES_HPP

#include <net.hpp>   // for llarp::Addr , llarp::huint32_t
#include <util/buffer.h>  // for byte_t

#include <vector>

namespace llarp
{
  namespace dns
  {
    struct record
    {
      virtual ~record() = 0;
      record()
      {
      }

      virtual bool
      parse(std::vector< byte_t > bytes) = 0;

      virtual std::vector< byte_t >
      to_bytes() = 0;
    };

    struct type_1a : public record
    {
      huint32_t ipaddr;

      virtual ~type_1a(){};
      type_1a();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_2ns : public record
    {
      std::string ns;

      virtual ~type_2ns(){};
      type_2ns();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_6soa : public record
    {
      std::string mname;
      std::string rname;
      uint32_t serial;
      uint32_t refresh;
      uint32_t retry;
      uint32_t expire;
      uint32_t minimum;

      virtual ~type_6soa(){};
      type_6soa();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_5cname : public record
    {
      std::string cname;

      virtual ~type_5cname(){};
      type_5cname();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_12ptr : public record
    {
      std::string revname;

      virtual ~type_12ptr(){};
      type_12ptr();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_15mx : public record
    {
      std::string mx;
      uint16_t priority;

      virtual ~type_15mx(){};
      type_15mx();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

    struct type_16txt : public record
    {
      std::string txt;

      virtual ~type_16txt(){};
      type_16txt();

      bool
      parse(std::vector< byte_t > bytes) override;

      std::vector< byte_t >
      to_bytes() override;
    };

  }  // namespace dns
}  // namespace llarp

#endif

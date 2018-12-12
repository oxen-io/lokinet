#ifndef LLARP_DTLS_HPP
#define LLARP_DTLS_HPP

#include <openssl/ssl.h>

namespace llarp
{
  namespace dtls
  {
    struct Base
    {
      Base(uint16_t mtu)
      {
        _ctx = SSL_CTX_new(DTLS_with_buffers_method());
        SSL_CTX_set_custom_verify(_ctx, SSL_VERIFY_PEER, []());
      }

      ~Base()
      {
        if(_ctx)
          SSL_CTX_free(_ctx);
      }

      SSL_CTX* _ctx = nullptr;
    };

  }  // namespace dtls

}  // namespace llarp

#endif

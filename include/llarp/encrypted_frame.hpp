#ifndef LLARP_ENCRYPTED_FRAME_HPP
#define LLARP_ENCRYPTED_FRAME_HPP

#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/threadpool.h>

namespace llarp
{
  struct EncryptedFrame
  {
    EncryptedFrame();
    EncryptedFrame(const byte_t* buf, size_t sz);
    ~EncryptedFrame();

    bool
    DecryptInPlace(const byte_t* ourSecretKey, llarp_crypto* crypto);

    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

   private:
    byte_t* m_Buf;
    size_t m_Sz;
    llarp_buffer_t m_Buffer;
  };

  template < typename User >
  struct AsyncFrameDecrypter
  {
    typedef void (*DecryptHandler)(llarp_buffer_t*, User*);

    static void
    Decrypt(void* user)
    {
      AsyncFrameDecrypter< User >* ctx =
          static_cast< AsyncFrameDecrypter< User >* >(user);

      if(ctx->target->DecryptInPlace(ctx->seckey, ctx->crypto))
        ctx->result(ctx->target->Buffer(), ctx->context);
      else
        ctx->result(nullptr, ctx->context);
    }

    AsyncFrameDecrypter(llarp_crypto* c, const byte_t* secretkey,
                        DecryptHandler h)
        : result(h), crypto(c), seckey(secretkey)
    {
    }

    DecryptHandler result;
    User* context;
    llarp_crypto* crypto;
    const byte_t* seckey;
    EncryptedFrame* target;
    void
    AsyncDecrypt(llarp_threadpool* worker, EncryptedFrame* frame, User* user)
    {
      target  = frame;
      context = user;
      llarp_threadpool_queue_job(worker, {this, &Decrypt});
    }
  };
}

#endif

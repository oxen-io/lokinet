#ifndef LLARP_ENCRYPTED_FRAME_HPP
#define LLARP_ENCRYPTED_FRAME_HPP

#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/threadpool.h>
#include <llarp/encrypted.hpp>

namespace llarp
{
  struct EncryptedFrame : public Encrypted
  {
    static constexpr size_t OverheadSize =
      PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE;
    EncryptedFrame() = default;
    EncryptedFrame(byte_t* buf, size_t sz) : Encrypted(buf, sz)
    {
    }
    EncryptedFrame(size_t sz)
        : Encrypted(sz + PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE)
    {
    }

    bool
    DecryptInPlace(byte_t* seckey, llarp_crypto* crypto);

    bool
    EncryptInPlace(byte_t* seckey, byte_t* other, llarp_crypto* crypto);
  };

  /// TOOD: can only handle 1 frame at a time
  template < typename User >
  struct AsyncFrameEncrypter
  {
    typedef void (*EncryptHandler)(EncryptedFrame*, User*);

    static void
    Encrypt(void* user)
    {
      AsyncFrameEncrypter< User >* ctx =
          static_cast< AsyncFrameEncrypter< User >* >(user);

      if(ctx->frame->EncryptInPlace(ctx->seckey, ctx->otherKey, ctx->crypto))
        ctx->handler(ctx->frame, ctx->user);
      else
      {
        delete ctx->frame;
        ctx->handler(nullptr, ctx->user);
      }
    }

    llarp_crypto* crypto;
    byte_t* secretkey;
    EncryptHandler handler;
    EncryptedFrame* frame;
    User* user;
    byte_t* otherKey;

    AsyncFrameEncrypter(llarp_crypto* c, byte_t* seckey, EncryptHandler h)
        : crypto(c), secretkey(seckey), handler(h)
    {
    }

    void
    AsyncEncrypt(llarp_threadpool* worker, llarp_buffer_t buf, byte_t* other,
                 User* u)
    {
      // TODO: should we own otherKey?
      otherKey = other;
      frame    = new EncryptedFrame(buf.sz);
      memcpy(frame->data + PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE, buf.base,
             buf.sz);
      user = u;
      llarp_threadpool_queue_job(worker, {this, &Encrypt});
    }
  };

  /// TOOD: can only handle 1 frame at a time
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

    AsyncFrameDecrypter(llarp_crypto* c, byte_t* secretkey, DecryptHandler h)
        : result(h), crypto(c), seckey(secretkey)
    {
    }

    DecryptHandler result;
    User* context;
    llarp_crypto* crypto;
    byte_t* seckey;
    EncryptedFrame* target;
    void
    AsyncDecrypt(llarp_threadpool* worker, EncryptedFrame* frame, User* user)
    {
      target  = frame;
      context = user;
      llarp_threadpool_queue_job(worker, {this, &Decrypt});
    }
  };
}  // namespace llarp

#endif

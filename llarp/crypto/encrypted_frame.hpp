#ifndef LLARP_ENCRYPTED_FRAME_HPP
#define LLARP_ENCRYPTED_FRAME_HPP

#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <util/buffer.hpp>
#include <util/mem.h>
#include <util/threadpool.h>

namespace llarp
{
  struct Crypto;

  static constexpr size_t EncryptedFrameOverheadSize =
      PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE;
  static constexpr size_t EncryptedFrameBodySize = 1024;
  static constexpr size_t EncryptedFrameSize =
      EncryptedFrameOverheadSize + EncryptedFrameBodySize;

  struct EncryptedFrame : public Encrypted< EncryptedFrameSize >
  {
    EncryptedFrame() : EncryptedFrame(EncryptedFrameBodySize)
    {
    }

    EncryptedFrame(size_t sz)
        : Encrypted< EncryptedFrameSize >(std::min(sz, EncryptedFrameBodySize)
                                          + EncryptedFrameOverheadSize)
    {
      UpdateBuffer();
    }

    EncryptedFrame&
    operator=(const EncryptedFrame& other)
    {
      _sz = other._sz;
      memcpy(data(), other.data(), size());
      UpdateBuffer();
      return *this;
    }

    void
    Resize(size_t sz)
    {
      if(sz <= EncryptedFrameSize)
      {
        _sz = sz;
        UpdateBuffer();
      }
    }

    bool
    DecryptInPlace(const SecretKey& seckey, llarp::Crypto* crypto);

    bool
    EncryptInPlace(const SecretKey& seckey, const PubKey& other,
                   llarp::Crypto* crypto);
  };

  /// TODO: can only handle 1 frame at a time
  template < typename User >
  struct AsyncFrameEncrypter
  {
    using EncryptHandler = std::function< void(EncryptedFrame*, User*) >;

    static void
    Encrypt(void* user)
    {
      AsyncFrameEncrypter< User >* ctx =
          static_cast< AsyncFrameEncrypter< User >* >(user);

      if(ctx->frame.EncryptInPlace(ctx->seckey, ctx->otherKey, ctx->crypto))
        ctx->handler(&ctx->frame, ctx->user);
      else
      {
        ctx->handler(nullptr, ctx->user);
      }
    }

    llarp::Crypto* crypto;
    byte_t* secretkey;
    EncryptHandler handler;
    EncryptedFrame frame;
    User* user;
    byte_t* otherKey;

    AsyncFrameEncrypter(llarp::Crypto* c, byte_t* seckey, EncryptHandler h)
        : crypto(c), secretkey(seckey), handler(h)
    {
    }

    void
    AsyncEncrypt(llarp_threadpool* worker, llarp_buffer_t buf, byte_t* other,
                 User* u)
    {
      // TODO: should we own otherKey?
      otherKey = other;
      if(buf.sz > EncryptedFrameBodySize)
        return;
      memcpy(frame.data() + EncryptedFrameOverheadSize, buf.base, buf.sz);
      user = u;
      llarp_threadpool_queue_job(worker, {this, &Encrypt});
    }
  };

  /// TODO: can only handle 1 frame at a time
  template < typename User >
  struct AsyncFrameDecrypter
  {
    using DecryptHandler = std::function< void(llarp_buffer_t*, User*) >;

    static void
    Decrypt(void* user)
    {
      AsyncFrameDecrypter< User >* ctx =
          static_cast< AsyncFrameDecrypter< User >* >(user);

      if(ctx->target.DecryptInPlace(ctx->seckey, ctx->crypto))
      {
        auto buf = ctx->target.Buffer();
        buf->cur = buf->base + EncryptedFrameOverheadSize;
        ctx->result(buf, ctx->context);
      }
      else
        ctx->result(nullptr, ctx->context);
    }

    AsyncFrameDecrypter(llarp::Crypto* c, const SecretKey& secretkey,
                        DecryptHandler h)
        : result(h), crypto(c), seckey(secretkey)
    {
    }

    DecryptHandler result;
    User* context;
    llarp::Crypto* crypto;
    const SecretKey& seckey;
    EncryptedFrame target;

    void
    AsyncDecrypt(llarp_threadpool* worker, const EncryptedFrame& frame,
                 User* user)
    {
      target  = frame;
      context = user;
      llarp_threadpool_queue_job(worker, {this, &Decrypt});
    }
  };
}  // namespace llarp

#endif

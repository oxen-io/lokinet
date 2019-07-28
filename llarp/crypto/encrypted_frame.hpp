#ifndef LLARP_ENCRYPTED_FRAME_HPP
#define LLARP_ENCRYPTED_FRAME_HPP

#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <util/buffer.hpp>
#include <util/mem.h>
#include <util/threadpool.h>

namespace llarp
{
  static constexpr size_t EncryptedFrameOverheadSize =
      PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE;
  static constexpr size_t EncryptedFrameBodySize = 128 * 6;
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
    DoEncrypt(const SharedSecret& shared, bool noDH = false);

    bool
    DecryptInPlace(const SecretKey& seckey);

    bool
    DoDecrypt(const SharedSecret& shared);

    bool
    EncryptInPlace(const SecretKey& seckey, const PubKey& other);
  };

  /// TODO: can only handle 1 frame at a time
  template < typename User >
  struct AsyncFrameDecrypter
  {
    using User_ptr       = std::shared_ptr< User >;
    using DecryptHandler = std::function< void(llarp_buffer_t*, User_ptr) >;

    static void
    Decrypt(void* user)
    {
      AsyncFrameDecrypter< User >* ctx =
          static_cast< AsyncFrameDecrypter< User >* >(user);

      if(ctx->target.DecryptInPlace(ctx->seckey))
      {
        auto buf = ctx->target.Buffer();
        buf->cur = buf->base + EncryptedFrameOverheadSize;
        ctx->result(buf, ctx->user);
      }
      else
        ctx->result(nullptr, ctx->user);
      ctx->user = nullptr;
    }

    AsyncFrameDecrypter(const SecretKey& secretkey, DecryptHandler h)
        : result(h), seckey(secretkey)
    {
    }

    DecryptHandler result;
    User_ptr user;
    const SecretKey& seckey;
    EncryptedFrame target;

    void
    AsyncDecrypt(const std::shared_ptr< thread::ThreadPool >& worker,
                 const EncryptedFrame& frame, User_ptr u)
    {
      target = frame;
      user   = u;
      worker->addJob(std::bind(&Decrypt, this));
    }
  };
}  // namespace llarp

#endif

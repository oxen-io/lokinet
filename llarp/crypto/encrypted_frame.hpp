#pragma once

#include "encrypted.hpp"
#include "types.hpp"
#include <llarp/util/buffer.hpp>
#include <utility>
#include <llarp/util/mem.h>

namespace llarp
{
  static constexpr size_t EncryptedFrameOverheadSize = PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE;
  static constexpr size_t EncryptedFrameBodySize = 128 * 6;
  static constexpr size_t EncryptedFrameSize = EncryptedFrameOverheadSize + EncryptedFrameBodySize;

  struct EncryptedFrame : public Encrypted<EncryptedFrameSize>
  {
    EncryptedFrame() : EncryptedFrame(EncryptedFrameBodySize)
    {}

    EncryptedFrame(size_t sz)
        : Encrypted<EncryptedFrameSize>(
            std::min(sz, EncryptedFrameBodySize) + EncryptedFrameOverheadSize)
    {}

    void
    Resize(size_t sz)
    {
      if (sz <= EncryptedFrameSize)
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
  template <typename User>
  struct AsyncFrameDecrypter
  {
    using User_ptr = std::shared_ptr<User>;
    using DecryptHandler = std::function<void(llarp_buffer_t*, User_ptr)>;

    void
    Decrypt(User_ptr user)
    {
      if (target.DecryptInPlace(seckey))
      {
        auto buf = target.Buffer();
        buf->cur = buf->base + EncryptedFrameOverheadSize;
        result(buf, user);
      }
      else
        result(nullptr, user);
    }

    AsyncFrameDecrypter(const SecretKey& secretkey, DecryptHandler h)
        : result(std::move(h)), seckey(secretkey)
    {}

    DecryptHandler result;
    const SecretKey& seckey;
    EncryptedFrame target;

    using WorkFunc_t = std::function<void(void)>;
    using WorkerFunction_t = std::function<void(WorkFunc_t)>;

    void
    AsyncDecrypt(const EncryptedFrame& frame, User_ptr u, WorkerFunction_t worker)
    {
      target = frame;
      worker(std::bind(&AsyncFrameDecrypter<User>::Decrypt, this, std::move(u)));
    }
  };
}  // namespace llarp

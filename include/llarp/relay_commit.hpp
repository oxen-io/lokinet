#ifndef LLARP_RELAY_COMMIT_HPP
#define LLARP_RELAY_COMMIT_HPP
#include <llarp/crypto.h>
#include <llarp/encrypted_frame.hpp>
#include <vector>

namespace llarp
{
  struct LR_CommitRecord
  {
    llarp_pubkey_t commkey;
    llarp_pubkey_t nextHop;
    llarp_tunnel_nonce_t nonce;
    uint64_t lifetime;
    uint64_t pathid;
    uint64_t version;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf);
  };

  struct LR_AcceptRecord
  {
    uint64_t pathid;
    uint64_t version;
    std::vector< byte_t > padding;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf);
  };

  struct LR_StatusMessage
  {
    std::vector< EncryptedFrame > replies;
    uint64_t version;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf);
  };

  struct LR_CommitMessage
  {
    std::vector< EncryptedFrame > frames;
    uint64_t version;

    void
    Clear();

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf);
  };

  struct AsyncPathDecryption
  {
    static void
    Decrypted(void *data);

    LR_CommitMessage lrcm;
    LR_CommitRecord ourRecord;
    llarp_threadpool *worker = nullptr;
    llarp_crypto *crypto     = nullptr;
    llarp_logic *logic       = nullptr;
    llarp_thread_job result;

    void
    AsyncDecryptOurHop();
  };

  struct AsyncPathEncryption
  {
    static void
    EncryptedFrame(void *data);

    std::vector< LR_CommitRecord > hops;
    LR_CommitMessage lrcm;
    llarp_threadpool *worker = nullptr;
    llarp_crypto *crypto     = nullptr;
    llarp_logic *logic       = nullptr;
    llarp_thread_job result;

    void
    AsyncEncrypt();

   private:
    void
    EncryptNext();
  };
}

#endif

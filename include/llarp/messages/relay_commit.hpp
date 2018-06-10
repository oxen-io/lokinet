#ifndef LLARP_RELAY_COMMIT_HPP
#define LLARP_RELAY_COMMIT_HPP
#include <llarp/crypto.h>
#include <llarp/encrypted_frame.hpp>
#include <llarp/link_message.hpp>
#include <llarp/path_types.hpp>
#include <vector>

namespace llarp
{
  struct LR_CommitRecord
  {
    llarp_pubkey_t commkey;
    llarp_pubkey_t nextHop;
    llarp_tunnel_nonce_t tunnelNonce;
    PathID_t txid;
    PathSymKey_t downstreamReplyKey;
    SymmNonce_t downstreamReplyNonce;
    uint64_t version;

    bool
    BDecode(llarp_buffer_t *buf);

    static bool
    OnKey(dict_reader *r, llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  struct LR_AcceptRecord
  {
    uint64_t pathid;
    uint64_t version;
    std::vector< byte_t > padding;

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  struct LR_StatusMessage
  {
    std::vector< EncryptedFrame > replies;
    uint64_t version;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;
  };

  struct LR_CommitMessage : public ILinkMessage
  {
    std::vector< EncryptedFrame > frames;
    uint64_t version;

    LR_CommitMessage(const RouterID &from) : ILinkMessage(from)
    {
    }
    ~LR_CommitMessage();

    void
    Clear();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;

    bool
    HandleMessage(llarp_router *router) const;
  };
}

#endif

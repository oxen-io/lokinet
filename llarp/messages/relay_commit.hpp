#ifndef LLARP_RELAY_COMMIT_HPP
#define LLARP_RELAY_COMMIT_HPP

#include <crypto/encrypted_frame.hpp>
#include <crypto/types.hpp>
#include <messages/link_message.hpp>
#include <path/path_types.hpp>
#include <pow.hpp>

#include <array>
#include <memory>

namespace llarp
{
  // forward declare
  struct AbstractRouter;
  namespace path
  {
    struct PathContext;
  }

  struct LR_CommitRecord
  {
    PubKey commkey;
    RouterID nextHop;
    TunnelNonce tunnelNonce;
    PathID_t txid, rxid;

    std::unique_ptr< RouterContact > nextRC;
    std::unique_ptr< PoW > work;
    uint64_t version  = 0;
    uint64_t lifetime = 0;

    bool
    BDecode(llarp_buffer_t *buf);

    bool
    BEncode(llarp_buffer_t *buf) const;

    bool
    operator==(const LR_CommitRecord &other) const;

   private:
    static bool
    OnKey(dict_reader *r, llarp_buffer_t *buf);
  };

  struct LR_CommitMessage : public ILinkMessage
  {
    std::array< EncryptedFrame, 8 > frames;

    LR_CommitMessage() : ILinkMessage()
    {
    }

    ~LR_CommitMessage();

    void
    Clear() override;

    bool
    DecodeKey(const llarp_buffer_t &key, llarp_buffer_t *buf) override;

    bool
    BEncode(llarp_buffer_t *buf) const override;

    bool
    HandleMessage(AbstractRouter *router) const override;

    bool
    AsyncDecrypt(llarp::path::PathContext *context) const;

    const char *
    Name() const override
    {
      return "RelayCommit";
    }
  };
}  // namespace llarp

#endif

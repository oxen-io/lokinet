#pragma once

#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/crypto/types.hpp>
#include "link_message.hpp"
#include <llarp/path/path_types.hpp>
#include <llarp/pow.hpp>

#include <array>
#include <memory>
#include <utility>

namespace llarp
{
  // forward declare
  struct AbstractRouter;
  namespace path
  {
    struct PathContext;
    struct IHopHandler;
    struct TransitHop;
  }  // namespace path

  struct LR_StatusRecord
  {
    static constexpr uint64_t SUCCESS = 1 << 0;
    static constexpr uint64_t FAIL_TIMEOUT = 1 << 1;
    static constexpr uint64_t FAIL_CONGESTION = 1 << 2;
    static constexpr uint64_t FAIL_DEST_UNKNOWN = 1 << 3;
    static constexpr uint64_t FAIL_DECRYPT_ERROR = 1 << 4;
    static constexpr uint64_t FAIL_MALFORMED_RECORD = 1 << 5;
    static constexpr uint64_t FAIL_DEST_INVALID = 1 << 6;
    static constexpr uint64_t FAIL_CANNOT_CONNECT = 1 << 7;
    static constexpr uint64_t FAIL_DUPLICATE_HOP = 1 << 8;

    uint64_t status = 0;
    uint64_t version = 0;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    operator==(const LR_StatusRecord& other) const;

   private:
    bool
    OnKey(llarp_buffer_t* buffer, llarp_buffer_t* key);
  };

  std::string
  LRStatusCodeToString(uint64_t status);

  struct LR_StatusMessage : public ILinkMessage
  {
    std::array<EncryptedFrame, 8> frames;

    PathID_t pathid;

    uint64_t status = 0;

    LR_StatusMessage(std::array<EncryptedFrame, 8> _frames)
        : ILinkMessage(), frames(std::move(_frames))
    {}

    LR_StatusMessage() = default;

    ~LR_StatusMessage() override = default;

    void
    Clear() override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    void
    SetDummyFrames();

    static bool
    CreateAndSend(
        AbstractRouter* router,
        std::shared_ptr<path::TransitHop> hop,
        const PathID_t pathid,
        const RouterID nextHop,
        const SharedSecret pathKey,
        uint64_t status);

    bool
    AddFrame(const SharedSecret& pathKey, uint64_t newStatus);

    static void
    QueueSendMessage(
        AbstractRouter* router,
        const RouterID nextHop,
        std::shared_ptr<LR_StatusMessage> msg,
        std::shared_ptr<path::TransitHop> hop);

    static void
    SendMessage(
        AbstractRouter* router, const RouterID nextHop, std::shared_ptr<LR_StatusMessage> msg);

    const char*
    Name() const override
    {
      return "RelayStatus";
    }
    virtual uint16_t
    Priority() const override
    {
      return 6;
    }
  };
}  // namespace llarp

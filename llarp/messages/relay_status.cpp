#include "relay_status.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/path/ihophandler.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_confirm_message.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/tooling/path_event.hpp>

#include <functional>
#include <utility>

namespace llarp
{
  struct LRSM_AsyncHandler : public std::enable_shared_from_this<LRSM_AsyncHandler>
  {
    using HopHandler_ptr = std::shared_ptr<llarp::path::IHopHandler>;

    std::array<EncryptedFrame, 8> frames;
    uint64_t status = 0;
    HopHandler_ptr hop;
    AbstractRouter* router;
    PathID_t pathid;

    LRSM_AsyncHandler(
        std::array<EncryptedFrame, 8> _frames,
        uint64_t _status,
        HopHandler_ptr _hop,
        AbstractRouter* _router,
        const PathID_t& pathid)
        : frames{std::move(_frames)}
        , status{_status}
        , hop{std::move(_hop)}
        , router{_router}
        , pathid{pathid}
    {}

    ~LRSM_AsyncHandler() = default;

    void
    handle()
    {
      router->NotifyRouterEvent<tooling::PathStatusReceivedEvent>(router->pubkey(), pathid, status);
      hop->HandleLRSM(status, frames, router);
    }

    void
    queue_handle()
    {
      auto func = std::bind(&llarp::LRSM_AsyncHandler::handle, shared_from_this());
      router->QueueWork(func);
    }
  };

  bool
  LR_StatusMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (key == "c")
    {
      return BEncodeReadArray(frames, buf);
    }
    if (key == "p")
    {
      if (!BEncodeMaybeReadDictEntry("p", pathid, read, key, buf))
      {
        return false;
      }
    }
    else if (key == "s")
    {
      if (!BEncodeMaybeReadDictInt("s", status, read, key, buf))
      {
        return false;
      }
    }
    else if (key == "v")
    {
      if (!BEncodeMaybeVerifyVersion("v", version, LLARP_PROTO_VERSION, read, key, buf))
      {
        return false;
      }
    }

    return read;
  }

  void
  LR_StatusMessage::Clear()
  {
    std::for_each(frames.begin(), frames.end(), [](auto& f) { f.Clear(); });
    version = 0;
    status = 0;
  }

  bool
  LR_StatusMessage::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;
    // msg type
    if (!BEncodeWriteDictMsgType(buf, "a", "s"))
      return false;
    // frames
    if (!BEncodeWriteDictArray("c", frames, buf))
      return false;
    // path id
    if (!BEncodeWriteDictEntry("p", pathid, buf))
      return false;
    // status (for now, only success bit is relevant)
    if (!BEncodeWriteDictInt("s", status, buf))
      return false;
    // version
    if (!bencode_write_uint64_entry(buf, "v", 1, LLARP_PROTO_VERSION))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_StatusMessage::HandleMessage(AbstractRouter* router) const
  {
    llarp::LogDebug("Received LR_Status message from (", session->GetPubKey(), ")");
    if (frames.size() != path::max_len)
    {
      llarp::LogError("LRSM invalid number of records, ", frames.size(), "!=", path::max_len);
      return false;
    }

    auto path = router->pathContext().GetByUpstream(session->GetPubKey(), pathid);
    if (not path)
    {
      llarp::LogWarn("unhandled LR_Status message: no associated path found pathid=", pathid);
      return false;
    }
    auto handler = std::make_shared<LRSM_AsyncHandler>(frames, status, path, router, pathid);
    handler->queue_handle();
    return true;
  }

  void
  LR_StatusMessage::SetDummyFrames()
  {
    // TODO
    return;
  }

  // call this from a worker thread
  bool
  LR_StatusMessage::CreateAndSend(
      AbstractRouter* router,
      std::shared_ptr<path::TransitHop> hop,
      const PathID_t pathid,
      const RouterID nextHop,
      const SharedSecret pathKey,
      uint64_t status)
  {
    auto message = std::make_shared<LR_StatusMessage>();

    message->status = status;
    message->pathid = pathid;

    message->SetDummyFrames();

    message->AddFrame(pathKey, status);

    QueueSendMessage(router, nextHop, message, hop);
    return true;  // can't guarantee delivery here, as far as we know it's fine
  }

  bool
  LR_StatusMessage::AddFrame(const SharedSecret& pathKey, uint64_t newStatus)
  {
    frames[7] = frames[6];
    frames[6] = frames[5];
    frames[5] = frames[4];
    frames[4] = frames[3];
    frames[3] = frames[2];
    frames[2] = frames[1];
    frames[1] = frames[0];

    auto& frame = frames[0];

    frame.Randomize();

    LR_StatusRecord record;

    record.status = newStatus;
    record.version = LLARP_PROTO_VERSION;

    llarp_buffer_t buf(frame.data(), frame.size());
    buf.cur = buf.base + EncryptedFrameOverheadSize;
    // encode record
    if (!record.BEncode(&buf))
    {
      // failed to encode?
      LogError(Name(), " Failed to generate Status Record");
      DumpBuffer(buf);
      return false;
    }
    // use ephemeral keypair for frame
    if (!frame.DoEncrypt(pathKey, true))
    {
      LogError(Name(), " Failed to encrypt LRSR");
      DumpBuffer(buf);
      return false;
    }

    return true;
  }

  void
  LR_StatusMessage::QueueSendMessage(
      AbstractRouter* router,
      const RouterID nextHop,
      std::shared_ptr<LR_StatusMessage> msg,
      std::shared_ptr<path::TransitHop> hop)
  {
    router->loop()->call([router, nextHop, msg = std::move(msg), hop = std::move(hop)] {
      SendMessage(router, nextHop, msg);
      // destroy hop as needed
      if ((msg->status & LR_StatusRecord::SUCCESS) != LR_StatusRecord::SUCCESS)
      {
        hop->QueueDestroySelf(router);
      }
    });
  }

  void
  LR_StatusMessage::SendMessage(
      AbstractRouter* router, const RouterID nextHop, std::shared_ptr<LR_StatusMessage> msg)
  {
    llarp::LogDebug("Attempting to send LR_Status message to (", nextHop, ")");
    if (not router->SendToOrQueue(nextHop, *msg))
    {
      llarp::LogError("Sending LR_Status message, SendToOrQueue to ", nextHop, " failed");
    }
  }

  bool
  LR_StatusRecord::BEncode(llarp_buffer_t* buf) const
  {
    return bencode_start_dict(buf) && BEncodeWriteDictInt("s", status, buf)
        && bencode_write_uint64_entry(buf, "v", 1, LLARP_PROTO_VERSION) && bencode_end(buf);
  }

  bool
  LR_StatusRecord::OnKey(llarp_buffer_t* buffer, llarp_buffer_t* key)
  {
    if (!key)
      return true;

    bool read = false;

    if (!BEncodeMaybeReadDictInt("s", status, read, *key, buffer))
      return false;
    if (!BEncodeMaybeVerifyVersion("v", version, LLARP_PROTO_VERSION, read, *key, buffer))
      return false;

    return read;
  }

  bool
  LR_StatusRecord::BDecode(llarp_buffer_t* buf)
  {
    return bencode_read_dict(util::memFn(&LR_StatusRecord::OnKey, this), buf);
  }

  bool
  LR_StatusRecord::operator==(const LR_StatusRecord& other) const
  {
    return status == other.status;
  }

  std::string
  LRStatusCodeToString(uint64_t status)
  {
    std::map<uint64_t, std::string> codes = {
        {LR_StatusRecord::SUCCESS, "success"},
        {LR_StatusRecord::FAIL_TIMEOUT, "timeout"},
        {LR_StatusRecord::FAIL_CONGESTION, "congestion"},
        {LR_StatusRecord::FAIL_DEST_UNKNOWN, "destination unknown"},
        {LR_StatusRecord::FAIL_DECRYPT_ERROR, "decrypt error"},
        {LR_StatusRecord::FAIL_MALFORMED_RECORD, "malformed record"},
        {LR_StatusRecord::FAIL_DEST_INVALID, "destination invalid"},
        {LR_StatusRecord::FAIL_CANNOT_CONNECT, "cannot connect"},
        {LR_StatusRecord::FAIL_DUPLICATE_HOP, "duplicate hop"}};
    std::stringstream ss;
    ss << "[";
    bool found = false;
    for (const auto& [val, message] : codes)
    {
      if ((status & val) == val)
      {
        ss << (found ? ", " : "") << message;
        found = true;
      }
    }
    ss << "]";
    return ss.str();
  }

}  // namespace llarp

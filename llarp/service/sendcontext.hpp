#pragma once
#include "intro.hpp"

#include <llarp/path/pathset.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/service/convotag.hpp>
#include "protocol.hpp"
#include <llarp/util/buffer.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/thread/queue.hpp>

#include <deque>

namespace llarp::service
{
  struct ServiceInfo;
  struct Endpoint;
  struct Introduction;

  struct SendContext
  {
    SendContext(ServiceInfo ident, const Introduction& intro, path::PathSet* send, Endpoint* ep);

    void
    AsyncEncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t);

    /// queue send a fully encrypted hidden service frame
    /// via a path
    bool
    Send(std::shared_ptr<ProtocolFrameMessage> f, path::Path_ptr path);

    /// flush upstream traffic when in router thread
    void
    FlushUpstream();

    SharedSecret sharedKey;
    ServiceInfo remoteIdent;
    Introduction remoteIntro;
    ConvoTag currentConvoTag;
    path::PathSet* const m_PathSet;
    Endpoint* const m_DataHandler;
    Endpoint* const m_Endpoint;
    uint64_t sequenceNo = 0;
    llarp_time_t lastGoodSend = 0s;
    const llarp_time_t createdAt;
    llarp_time_t sendTimeout = path::BUILD_TIMEOUT;
    llarp_time_t connectTimeout = path::BUILD_TIMEOUT * 2;
    llarp_time_t shiftTimeout = (path::BUILD_TIMEOUT * 5) / 2;
    llarp_time_t estimatedRTT = 0s;
    bool markedBad = false;
    using Msg_ptr = std::shared_ptr<routing::PathTransferMessage>;
    using SendEvent_t = std::pair<Msg_ptr, path::Path_ptr>;

    thread::Queue<SendEvent_t> m_SendQueue;

    std::function<void(AuthResult)> authResultListener;

    virtual bool
    ShiftIntroduction(bool rebuild = true)
    {
      (void)rebuild;
      return true;
    }

    virtual void
    ShiftIntroRouter(const RouterID) = 0;

    virtual void
    UpdateIntroSet() = 0;

    virtual void
    MarkCurrentIntroBad(llarp_time_t now) = 0;

    void
    AsyncSendAuth(std::function<void(AuthResult)> replyHandler);

   private:
    virtual bool
    IntroGenerated() const = 0;
    virtual bool
    IntroSent() const = 0;

    void
    EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t);

    virtual void
    AsyncGenIntro(const llarp_buffer_t& payload, ProtocolType t) = 0;
  };
}  // namespace llarp::service

#pragma once

#include <memory>

namespace llarp
{
  struct Router;

  namespace dht
  {
    struct AbstractDHTMessage;
  }

  namespace service
  {
    struct ProtocolFrameMessage;
  }

  namespace routing
  {
    struct DataDiscardMessage;
    struct GrantExitMessage;
    struct ObtainExitMessage;
    struct RejectExitMessage;
    struct TransferTrafficMessage;
    struct UpdateExitMessage;
    struct UpdateExitVerifyMessage;
    struct CloseExitMessage;
    struct PathTransferMessage;
    struct PathConfirmMessage;
    struct PathLatencyMessage;

    // handles messages on the routing level
    struct AbstractRoutingMessageHandler
    {
      virtual bool
      HandleObtainExitMessage(const ObtainExitMessage& msg, Router* r) = 0;

      virtual bool
      HandleGrantExitMessage(const GrantExitMessage& msg, Router* r) = 0;

      virtual bool
      HandleRejectExitMessage(const RejectExitMessage& msg, Router* r) = 0;

      virtual bool
      HandleTransferTrafficMessage(const TransferTrafficMessage& msg, Router* r) = 0;

      virtual bool
      HandleUpdateExitMessage(const UpdateExitMessage& msg, Router* r) = 0;

      virtual bool
      HandleUpdateExitVerifyMessage(const UpdateExitVerifyMessage& msg, Router* r) = 0;

      virtual bool
      HandleCloseExitMessage(const CloseExitMessage& msg, Router* r) = 0;

      virtual bool
      HandleDataDiscardMessage(const DataDiscardMessage& msg, Router* r) = 0;

      virtual bool
      HandlePathTransferMessage(const PathTransferMessage& msg, Router* r) = 0;

      virtual bool
      HandleHiddenServiceFrame(const service::ProtocolFrameMessage& msg) = 0;

      virtual bool
      HandlePathConfirmMessage(const PathConfirmMessage& msg, Router* r) = 0;

      virtual bool
      HandlePathLatencyMessage(const PathLatencyMessage& msg, Router* r) = 0;
      virtual bool
      HandleDHTMessage(const dht::AbstractDHTMessage& msg, Router* r) = 0;
    };

    using MessageHandler_ptr = std::shared_ptr<AbstractRoutingMessageHandler>;

  }  // namespace routing
}  // namespace llarp

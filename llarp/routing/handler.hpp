#pragma once

#include <memory>

namespace llarp
{
  struct AbstractRouter;

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
      HandleObtainExitMessage(const ObtainExitMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleGrantExitMessage(const GrantExitMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleRejectExitMessage(const RejectExitMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleTransferTrafficMessage(const TransferTrafficMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleUpdateExitMessage(const UpdateExitMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleUpdateExitVerifyMessage(const UpdateExitVerifyMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleCloseExitMessage(const CloseExitMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleDataDiscardMessage(const DataDiscardMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandlePathTransferMessage(const PathTransferMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandleHiddenServiceFrame(const service::ProtocolFrameMessage& msg) = 0;

      virtual bool
      HandlePathConfirmMessage(const PathConfirmMessage& msg, AbstractRouter* r) = 0;

      virtual bool
      HandlePathLatencyMessage(const PathLatencyMessage& msg, AbstractRouter* r) = 0;
      virtual bool
      HandleDHTMessage(const dht::AbstractDHTMessage& msg, AbstractRouter* r) = 0;
    };

    using MessageHandler_ptr = std::shared_ptr<AbstractRoutingMessageHandler>;

  }  // namespace routing
}  // namespace llarp

#pragma once

#include "common.hpp"

namespace llarp
{
  struct PathMessage : public AbstractSerializable
  {};

  struct RelayCommitMessage : public PathMessage
  {};

  struct RelayStatusMessage : public PathMessage
  {};

  struct PathConfirmMessage : public PathMessage
  {};

  struct PathLatencyMessage : public PathMessage
  {};

  struct PathTransferMessage : public PathMessage
  {};

}  // namespace llarp

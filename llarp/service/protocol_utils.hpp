#pragma once

#include <llarp/util/buffer.hpp>

namespace llarp::service
{
  /// collect many owned buffers and partition and pack them
  std::vector<OwnedBuffer>
  PackAll(std::vector<OwnedBuffer> packets);

  /// unpack packed buffers packed by PackAll
  std::vector<OwnedBuffer>
  Unpack(OwnedBuffer packets);

}  // namespace llarp::service

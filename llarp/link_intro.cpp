#include "link_handlers.hpp"

namespace llarp {
namespace frame {
bool process_intro(struct llarp_frame_handler* h, struct llarp_link_session* s,
                   llarp_buffer_t msg) {
  return false;
}
}  // namespace frame
}  // namespace llarp

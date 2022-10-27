#include <llarp/util/service_manager.hpp>

#if defined(WITH_SYSTEMD)
#include <systemd/sd-daemon.h>
#include <cassert>
#endif

namespace llarp::sys
{
#if defined(WITH_SYSTEMD)
  class SD_Manager : public I_SystemLayerManager
  {
    llarp::sys::ServiceState m_State{ServiceState::Initial};

   public:
    /// change our state and report it to the system layer
    void
    we_changed_our_state(ServiceState st) override
    {
      assert(m_State != st);
      m_State = st;
      report_our_state();
    }

    /// report our current state to the system layer
    void
    report_our_state() override
    {
      if (m_State == ServiceState::Starting)
      {
        // TODO: maybe request more time?
        ::sd_notify(0, "READY=0");
        return;
      }
      if (m_State == ServiceState::Running)
      {
        ::sd_notify(0, "READY=1");
        return;
      }
      if (m_State == ServiceState::Stopping)
      {
        ::sd_notify(0, "STOPPING=1");
        return;
      }
    }

    void
    system_changed_our_state(ServiceState) override
    {
      // not applicable on systemd
    }
  };

  SD_Manager _manager{};
#else
  NOP_SystemLayerHandler _manager{};
#endif

  I_SystemLayerManager* const service_manager = &_manager;

}  // namespace llarp::sys

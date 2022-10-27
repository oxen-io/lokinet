#pragma once
#include <llarp/util/service_manager.hpp>
namespace llarp::sys
{

  class SVC_Manager : public I_SystemLayerManager
  {
    SERVICE_STATUS _status;

   public:
    SERVICE_STATUS_HANDLE handle;

    SVC_Manager();

    void
    system_changed_our_state(ServiceState st) override;

    void
    report_changed_state() override;

    void
    we_changed_our_state(ServiceState st) override;
  };
}  // namespace llarp::sys

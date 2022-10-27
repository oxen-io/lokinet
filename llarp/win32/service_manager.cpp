#include <windows.h>
#include <llarp.hpp>
#include "service_manager.hpp"
#include <dbghelp.h>
#include <cassert>
#include <csignal>
#include <optional>

namespace llarp::sys
{

  namespace
  {

    std::optional<DWORD>
    to_win32_state(ServiceState st)
    {
      switch (st)
      {
        case ServiceState::Starting:
          return SERVICE_START_PENDING;
        case ServiceState::Running:
          return SERVICE_RUNNING;
        case ServiceState::Stopping:
          return SERVICE_STOP_PENDING;
        case ServiceState::Stopped:
          return SERVICE_STOPPED;
        default:
          return std::nullopt;
      }
    }
  }  // namespace

  SVC_Manager::SVC_Manager()
  {
    _status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  }

  void
  SVC_Manager::system_changed_our_state(ServiceState st)
  {
    if (m_disable)
      return;
    if (st == ServiceState::Stopping)
    {
      we_changed_our_state(st);
      m_Context->HandleSignal(SIGINT);
    }
  }

  void
  SVC_Manager::report_our_state()
  {
    if (m_disable)
      return;
    SetServiceStatus(handle, &_status);
  }

  void
  SVC_Manager::we_changed_our_state(ServiceState st)
  {
    if (st == ServiceState::Failed)
    {
      _status.dwWin32ExitCode = 2;  // TODO: propagate more info ?
      report_our_state();
    }
    else if (auto maybe_state = to_win32_state(st))
    {
      auto new_state = *maybe_state;
      assert(_status.dwCurrentState != new_state);
      _status.dwCurrentState = new_state;
      // tell windows it takes 5s at most to start or stop
      if (st == ServiceState::Starting or st == ServiceState::Stopping)
        _status.dwWaitHint = 5000;
      else  // other state changes are a half second
        _status.dwWaitHint = 500;
      report_our_state();
    }
  }

  SVC_Manager _manager{};
  I_SystemLayerManager* const service_manager = &_manager;
}  // namespace llarp::sys

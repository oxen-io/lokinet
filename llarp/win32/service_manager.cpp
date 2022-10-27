#include <windows.h>
#include <llarp.hpp>
#include <llarp/util/logging.hpp>
#include "service_manager.hpp"
#include <dbghelp.h>
#include <cassert>
#include <csignal>
#include <optional>

namespace llarp::sys
{

  static auto logcat = log::Cat("svc");

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
  SVC_Manager::report_changed_state()
  {
    if (m_disable)
      return;

    log::debug(
        logcat,
        "Reporting Windows service status '{}', exit code {}, wait hint {}, dwCP {}, dwCA {}",
        _status.dwCurrentState == SERVICE_START_PENDING ? "start pending"
            : _status.dwCurrentState == SERVICE_RUNNING ? "running"
            : _status.dwCurrentState == SERVICE_STOPPED ? "stopped"
            : _status.dwCurrentState == SERVICE_STOP_PENDING
            ? "stop pending"
            : fmt::format("unknown: {}", _status.dwCurrentState),
        _status.dwWin32ExitCode,
        _status.dwWaitHint,
        _status.dwCheckPoint,
        _status.dwControlsAccepted);

    SetServiceStatus(handle, &_status);
  }

  void
  SVC_Manager::we_changed_our_state(ServiceState st)
  {
    if (st == ServiceState::Failed)
    {
      _status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
      _status.dwServiceSpecificExitCode = 2;  // TODO: propagate more info ?
      report_changed_state();
    }
    else if (auto maybe_state = to_win32_state(st))
    {
      auto new_state = *maybe_state;
      assert(_status.dwCurrentState != new_state);
      _status.dwWin32ExitCode = NO_ERROR;
      _status.dwCurrentState = new_state;
      _status.dwControlsAccepted = st == ServiceState::Running ? SERVICE_ACCEPT_STOP : 0;
      // tell windows it takes 5s at most to start or stop
      if (st == ServiceState::Starting or st == ServiceState::Stopping)
        _status.dwCheckPoint++;
      else
        _status.dwCheckPoint = 0;

      report_changed_state();
    }
  }

  SVC_Manager _manager{};
  I_SystemLayerManager* const service_manager = &_manager;
}  // namespace llarp::sys

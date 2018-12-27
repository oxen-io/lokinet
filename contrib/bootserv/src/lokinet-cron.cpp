#include "lokinet-cron.hpp"

namespace lokinet
{
  namespace bootserv
  {
    CronHandler::CronHandler(std::ostream& o) : Handler(o)
    {
    }

    CronHandler::~CronHandler()
    {
    }

    int
    CronHandler::Exec(const Config& conf)
    {
      // this runs the cron tasks
      // TODO: implement me
      return 0;
    }

    int
    CronHandler::ReportError(const char* err)
    {
      out << "error: " << err << std::endl;
      return 1;
    }

    Handler_ptr
    NewCronHandler(std::ostream& out)
    {
      return std::make_unique< CronHandler >(out);
    }

  }  // namespace bootserv
}  // namespace lokinet

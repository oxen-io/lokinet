#include "handler.hpp"
#include "lokinet-config.hpp"

#include <getopt.h>
#include <string_view>
#include <sstream>

static int
printhelp(const char* exe)
{
  std::cout << "usage: " << exe << " [--cron] [--conf /path/to/alt/config.ini]"
            << std::endl;
  return 1;
}

int
main(int argc, char* argv[])
{
  bool RunCron = false;

  const char* confFile = lokinet::bootserv::Config::DefaultPath;
  lokinet::bootserv::Config config;

  lokinet::bootserv::Handler_ptr handler;

  option longopts[] = {{"cron", no_argument, 0, 'C'},
                       {"help", no_argument, 0, 'h'},
                       {"conf", required_argument, 0, 'c'},
                       {0, 0, 0, 0}};

  int c     = 0;
  int index = 0;
  while((c = getopt_long(argc, argv, "hCc:", longopts, &index)) != -1)
  {
    switch(c)
    {
      case 'h':
        return printhelp(argv[0]);
      case 'C':
        RunCron = true;
        break;
      case 'c':
        confFile = optarg;
        break;
    }
  }
  if(RunCron)
    handler = lokinet::bootserv::NewCronHandler(std::cout);
  else
    handler = lokinet::bootserv::NewCGIHandler(std::cout);

  if(!config.LoadFile(confFile))
  {
    std::stringstream ss;
    ss << "failed to load " << confFile;
    return handler->ReportError(ss.str().c_str());
  }
  else
    return handler->Exec(config);
}

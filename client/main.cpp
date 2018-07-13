#include <llarp/logger.h>
#include <llarp/api.hpp>
#include <llarp/logger.hpp>

int
main(int argc, char* argv[])
{
  cSetLogLevel(eLogDebug);
  std::string url = llarp::api::DefaultURL;
  if(argc > 1)
  {
    url = argv[1];
  }
  llarp::api::Client cl("hiddenservice");
  if(!cl.Start(url))
  {
    llarp::LogError("failed to start session");
    return 1;
  }
  return cl.Mainloop();
}
#include <llarp/api.hpp>

int
main(int argc, char* argv[])
{
  std::string url = llarp::api::DefaultURL;
  if(argc > 1)
  {
    url = argv[1];
  }
  llarp::api::Client cl;
  if(!cl.Start(url))
    return 1;
  return cl.Mainloop();
}
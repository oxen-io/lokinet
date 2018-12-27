#include "lokinet-cgi.hpp"
#include <fstream>
#include <dirent.h>
#include <list>
#include <sstream>

namespace lokinet
{
  namespace bootserv
  {
    CGIHandler::CGIHandler(std::ostream& o) : Handler(o)
    {
    }

    CGIHandler::~CGIHandler()
    {
    }

    int
    CGIHandler::Exec(const Config& conf)
    {
      const char* e = getenv("REQUEST_METHOD");
      if(e == nullptr)
        return ReportError("$REQUEST_METHOD not set");
      std::string_view method(e);

      if(method != "GET")
      {
        out << "Content-Type: text/plain" << std::endl;
        out << "Status: 405 Method Not Allowed" << std::endl << std::endl;
        return 0;
      }

      std::string fname;
      if(!conf.VisitSection(
             "nodedb", [&](const Config::Section_t& sect) -> bool {
               auto itr = sect.find("dir");
               if(itr == sect.end())
                 return false;
               fname = PickRandomFileInDir(
                   std::string(itr->second.data(), itr->second.size()));
               return true;
             }))

        return ReportError("bad values in nodedb section of config");
      if(fname.empty())
      {
        // no files in nodedb
        out << "Content-Type: text/plain" << std::endl;
        out << "Status: 404 Not Found" << std::endl << std::endl;
        return 0;
      }
      return ServeFile(fname.c_str(), "application/octect-stream");
    }

    std::string
    CGIHandler::PickRandomFileInDir(std::string dirname) const
    {
      // collect files
      std::list< std::string > files;
      {
        DIR* d = opendir(dirname.c_str());
        if(d == nullptr)
        {
          return "";
        };
        std::list< std::string > subdirs;
        dirent* ent = nullptr;
        while((ent = readdir(d)))
        {
          std::string_view f = ent->d_name;
          if(f != "." && f != "..")
          {
            std::stringstream ss;
            ss << dirname;
            ss << '/';
            ss << f;
            subdirs.emplace_back(ss.str());
          }
        }
        closedir(d);
        for(const auto& subdir : subdirs)
        {
          d = opendir(subdir.c_str());
          if(d)
          {
            while((ent = readdir(d)))
            {
              std::string_view f;
              f = ent->d_name;
              if(f != "." && f != ".."
                 && f.find_last_of(".signed") != std::string_view::npos)
              {
                std::stringstream ss;
                ss << subdir << "/" << f;
                files.emplace_back(ss.str());
              }
            }
            closedir(d);
          }
        }
      }
      uint32_t randint;
      {
        std::basic_ifstream< uint32_t > randf("/dev/urandom");
        if(!randf.is_open())
          return "";
        randf.read(&randint, 1);
      }
      auto itr = files.begin();
      if(files.size() > 1)
        std::advance(itr, randint % files.size());
      return *itr;
    }

    int
    CGIHandler::ServeFile(const char* fname, const char* contentType) const
    {
      std::ifstream f(fname);
      if(f.is_open())
      {
        f.seekg(0, std::ios::end);
        auto sz = f.tellg();
        f.seekg(0, std::ios::beg);
        if(sz)
        {
          out << "Content-Type: " << contentType << std::endl;
          out << "Status: 200 OK" << std::endl;
          out << "Content-Length: " << std::to_string(sz) << std::endl
              << std::endl;
          char buf[512] = {0};
          size_t r      = 0;
          while((r = f.readsome(buf, sizeof(buf))) > 0)
            out.write(buf, r);
          out << std::flush;
        }
        else
        {
          out << "Content-Type: text/plain" << std::endl;
          out << "Status: 500 Internal Server Error" << std::endl << std::endl;
          out << "could not serve '" << fname << "' as it is an empty file"
              << std::endl;
        }
      }
      else
      {
        out << "Content-Type: text/plain" << std::endl;
        out << "Status: 404 Not Found" << std::endl << std::endl;
        out << "could not serve '" << fname
            << "' as it does not exist on the filesystem" << std::endl;
      }
      return 0;
    }

    int
    CGIHandler::ReportError(const char* err)
    {
      out << "Content-Type: text/plain" << std::endl;
      out << "Status: 500 Internal Server Error" << std::endl << std::endl;
      out << err << std::endl;
      return 0;
    }

    Handler_ptr
    NewCGIHandler(std::ostream& out)
    {
      return std::make_unique< CGIHandler >(out);
    }

  }  // namespace bootserv
}  // namespace lokinet

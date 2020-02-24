#include <util/logging/json_logger.hpp>
#include <util/json.hpp>

namespace llarp
{
  void
  JSONLogStream::AppendLog(LogLevel lvl, const char* fname, int lineno,
                           const std::string& nodename, const std::string msg)
  {
    json::Object obj;
    obj["time"]     = llarp::time_now_ms().count();
    obj["nickname"] = nodename;
    obj["file"]     = std::string(fname);
    obj["line"]     = lineno;
    obj["level"]    = LogLevelToString(lvl);
    obj["message"]  = msg;
    m_Lines.pushBack(obj.dump());
  }

}  // namespace llarp

#include "json_logger.hpp"
#include <llarp/util/json.hpp>

namespace llarp
{
  void
  JSONLogStream::AppendLog(
      LogLevel lvl,
      const char* fname,
      int lineno,
      const std::string& nodename,
      const std::string msg)
  {
    json::Object obj;
    obj["time"] = to_json(llarp::time_now_ms());
    obj["nickname"] = nodename;
    obj["file"] = std::string(fname);
    obj["line"] = lineno;
    obj["level"] = LogLevelToString(lvl);
    obj["message"] = msg;
    m_Lines.pushBack(obj.dump());
  }

}  // namespace llarp

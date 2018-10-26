#include <abyss/json.hpp>
#include <vector>
#include <cstring>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace abyss
{
  namespace json
  {
    struct RapidJSONParser : public IParser
    {
      RapidJSONParser(size_t contentSize) : m_Buf(contentSize + 1), m_Offset(0)
      {
      }

      std::vector< char > m_Buf;
      size_t m_Offset;

      bool
      FeedData(const char* buf, size_t sz)
      {
        if(m_Offset + sz > m_Buf.size() - 1)
          return false;
        memcpy(m_Buf.data() + m_Offset, buf, sz);
        m_Offset += sz;
        m_Buf[m_Offset] = 0;
        return true;
      }

      Result
      Parse(Document& obj) const
      {
        if(m_Offset < m_Buf.size() - 1)
          return eNeedData;
        obj.Parse(m_Buf.data());
        if(obj.HasParseError())
          return eParseError;
        return eDone;
      }
    };

    IParser*
    MakeParser(size_t contentSize)
    {
      return new RapidJSONParser(contentSize);
    }

    void
    ToString(const json::Document& val, std::string& out)
    {
      rapidjson::StringBuffer s;
      rapidjson::Writer< rapidjson::StringBuffer > writer(s);
      val.Accept(writer);
      out = s.GetString();
    }

  }  // namespace json
}  // namespace abyss
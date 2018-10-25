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
      RapidJSONParser(size_t contentSize) : m_Buf(contentSize), m_Offset(0)
      {
      }

      std::vector< char > m_Buf;
      size_t m_Offset;

      bool
      FeedData(const char* buf, size_t sz)
      {
        if(m_Offset + sz > m_Buf.size())
          return false;
        memcpy(m_Buf.data() + m_Offset, buf, sz);
        m_Offset += sz;
        return true;
      }

      Result
      Parse(Document& obj) const
      {
        if(m_Offset < m_Buf.size())
          return eNeedData;
        obj.Parse(m_Buf.data(), m_Buf.size());
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
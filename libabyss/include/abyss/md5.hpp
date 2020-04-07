#ifndef MD5_HPP
#define MD5_HPP
#include <array>
#include <algorithm>
#include <string>

struct MD5
{
  MD5();

  void
  Update(const unsigned char* ptr, uint32_t len);
  void
  Final(uint8_t* digest);

  uint32_t i[2];        /* number of _bits_ handled mod 2^64 */
  uint32_t buf[4];      /* scratch buffer */
  unsigned char in[64]; /* input buffer */

  /// do md5(str) and return hex encoded digest
  static std::string
  SumHex(const std::string& str)
  {
    std::array<uint8_t, 16> digest;
    auto dist = str.size();
    MD5 m;
    m.Update((const unsigned char*)str.c_str(), dist);
    m.Final(digest.data());
    std::string hex;
    std::for_each(digest.begin(), digest.end(), [&hex](const unsigned char& ch) {
      char tmpbuf[4] = {0};
      std::snprintf(tmpbuf, sizeof(tmpbuf), "%.2x", ch);
      hex += std::string(tmpbuf);
    });
    return hex;
  }
};

#endif

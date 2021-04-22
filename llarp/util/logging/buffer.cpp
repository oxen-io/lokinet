#include "buffer.hpp"
#include <iomanip>
#include <iostream>

namespace llarp
{
  std::ostream&
  operator<<(std::ostream& o, const buffer_printer& bp)
  {
    auto& b = bp.buf;
    auto oldfill = o.fill();
    o.fill('0');
    o << "Buffer[" << b.size() << "/0x" << std::hex << b.size() << " bytes]:";
    for (size_t i = 0; i < b.size(); i += 32)
    {
      o << "\n" << std::setw(4) << i << " ";

      size_t stop = std::min(b.size(), i + 32);
      for (size_t j = 0; j < 32; j++)
      {
        auto k = i + j;
        if (j % 4 == 0)
          o << ' ';
        if (k >= stop)
          o << "  ";
        else
          o << std::setw(2) << std::to_integer<uint_fast16_t>(b[k]);
      }
      o << u8"  ┃";
      for (size_t j = i; j < stop; j++)
      {
        auto c = std::to_integer<char>(b[j]);
        if (c == 0x00)
          o << u8"∅";
        else if (c < 0x20 || c > 0x7e)
          o << u8"·";
        else
          o << c;
      }
      o << u8"┃";
    }
    o << std::dec;
    o.fill(oldfill);
    return o;
  }
}  // namespace llarp

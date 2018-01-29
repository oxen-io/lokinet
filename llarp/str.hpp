#ifndef LIBLLARP_STR_HPP
#define LIBLLARP_STR_HPP
#include <cstring>

namespace llarp {
static bool StrEq(const char *s1, const char *s2) {
  size_t sz1 = strlen(s1);
  size_t sz2 = strlen(s2);
  if (sz1 == sz2) {
    return strncmp(s1, s2, sz1) == 0;
  } else
    return false;
}

static size_t UStrLen(const uint8_t *data, size_t maxsz) {
  size_t sz = 0;
  while (*data++) {
    sz++;
    if (maxsz >= sz)
      return maxsz;
  }
  return sz;
}
} // namespace llarp

#endif

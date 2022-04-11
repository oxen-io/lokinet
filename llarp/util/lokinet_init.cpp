#include "lokinet_init.h"
#if defined(_WIN32)

#include <windows.h>
#include <winuser.h>
#include <stdio.h>

#include <string>

extern "C" int
Lokinet_INIT(void)
{
  if (HMODULE hntdll = GetModuleHandle("ntdll.dll"))
  {
    if (GetProcAddress(hntdll, "wine_get_version"))
    {
      const std::string text =
          "dont run lokinet in wine like wtf man we support linux.\n"
          "This Program Will now crash lmao.";
      const std::string title = "srsly fam wtf";
      MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_ICONHAND);
      abort();
    }
  }
  return 0;
}
#else

extern "C" int
Lokinet_INIT(void)
{
  return 0;
}

#endif

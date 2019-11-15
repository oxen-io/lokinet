#include <util/lokinet_init.h>
#if defined(_WIN32)
#include <windows.h>
#include <winuser.h>
#include <stdio.h>

int
Lokinet_INIT(void)
{
  static const char *(CDECL * pwine_get_version)(void);
  HMODULE hntdll = GetModuleHandle("ntdll.dll");
  if(hntdll)
  {
    pwine_get_version = (void *)GetProcAddress(hntdll, "wine_get_version");
    if(pwine_get_version)
    {
      static const char *text =
          "dont run lokinet in wine like wtf man we support linux and pretty "
          "much every flavor of BSD.\nThis Program Will now crash lmao.";
      static const char *title = "srsly fam wtf";
      MessageBoxA(NULL, text, title, MB_ICONEXCLAMATION | MB_OK);
      abort();
    }
  }
  return 0;
}
#else
int
Lokinet_INIT(void)
{
  return 0;
}

#endif
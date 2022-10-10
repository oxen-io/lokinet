#include "lokinet_init.h"

#if defined(_WIN32)
#include <windows.h>
#include <winuser.h>
#include <stdio.h>

#define WineKiller \
  DieInCaseSomehowThisGetsRunInWineButLikeWTFThatShouldNotHappenButJustInCaseHandleItWithAPopupOrSomeShit

struct WineKiller
{
  WineKiller()
  {
    if (auto hntdll = GetModuleHandle("ntdll.dll"))
    {
      if (GetProcAddress(hntdll, "wine_get_version"))
      {
        static const char* text =
            "dont run lokinet in wine like wtf man we support linux and pretty "
            "much every flavour of BSD, and even some flavours of unix system "
            "5.x.\nThis Program Will now crash lmao.";
        static const char* title = "srsly fam wtf";
        MessageBoxA(NULL, text, title, MB_ICONHAND);
        abort();
      }
    }
  }
};

// i heckin love static initalization
WineKiller lmao{};
#endif

extern "C" int
Lokinet_INIT(void)
{
  return 0;
}

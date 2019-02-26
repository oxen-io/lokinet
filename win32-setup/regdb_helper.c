/*
* Copyright (c)2018-2019 Rick V. All rights reserved.
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*------------------------------------------------------------------------------
* Shared object loaded by lokinet installer to properly detect the presence
* of the TAP v9 adapter
* -rick
*/

#include <sys/types.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows registry crap */
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define ETHER_ADDR_LEN 6

/* checks if TAP-Win32 v9 is already installed */
BOOL
reg_query_helper()
{
  HKEY adapters, adapter;
  DWORD i, ret, len;
  char *deviceid = NULL;
  DWORD sub_keys = 0;
  BOOL found = FALSE;

  ret =
      RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"), 0, KEY_READ, &adapters);
  if(ret != ERROR_SUCCESS)
    return FALSE;

  ret = RegQueryInfoKey(adapters, NULL, NULL, NULL, &sub_keys, NULL, NULL, NULL,
                        NULL, NULL, NULL, NULL);
  if(ret != ERROR_SUCCESS)
    return FALSE;

  if(sub_keys <= 0)
    return FALSE;

  /* Walk througt all adapters */
  for(i = 0; i < sub_keys; i++)
  {
    char new_key[MAX_KEY_LENGTH];
    char data[256];
    TCHAR key[MAX_KEY_LENGTH];
    DWORD keylen = MAX_KEY_LENGTH;

    /* Get the adapter key name */
    ret = RegEnumKeyEx(adapters, i, key, &keylen, NULL, NULL, NULL, NULL);
    if(ret != ERROR_SUCCESS)
      continue;

    /* Append it to NETWORK_ADAPTERS and open it */
    snprintf(new_key, sizeof new_key, "%s\\%s", "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", key);
    ret =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(new_key), 0, KEY_READ, &adapter);
    if(ret != ERROR_SUCCESS)
      continue;

    /* Check its values */
    len = sizeof data;
    ret =
        RegQueryValueEx(adapter, "ComponentId", NULL, NULL, (LPBYTE)data, &len);
    if(ret != ERROR_SUCCESS)
    {
      /* This value doesn't exist in this adaptater tree */
      goto clean;
    }
    /* If its a tap adapter, its all good */
	/* We only support TAP 9.x, TAP 8.x users must upgrade. */
    if(strncmp(data, "tap0901", 7) == 0)
    {
      DWORD type;

      len = sizeof data;
      ret = RegQueryValueEx(adapter, "NetCfgInstanceId", NULL, &type,
                            (LPBYTE)data, &len);
      if(ret != ERROR_SUCCESS)
        goto clean;
      found = TRUE;
      break;
    }
  clean:
    RegCloseKey(adapter);
  }
  RegCloseKey(adapters);
  return found;
}

#pragma once

#include "lokinet_export.h"

#ifdef __cplusplus
extern "C"
{
#endif

  struct lokinet_config;

  /// add config option to lokinet config
  void EXPORT
  lokinet_config_add_opt(
      struct lokinet_config*, const char* section, const char* key, const char* val);

#ifdef __cplusplus
}
#endif

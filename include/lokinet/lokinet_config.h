#pragma once

#include "lokinet_export.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

  struct lokinet_config;

  /// load a lokinet configuration from an open file handle.
  struct lokinet_config* EXPORT
  lokinet_config_from_file(FILE* f);

  /// free a lokinet config we loaded.
  void EXPORT
  lokinet_config_free(struct lokinet_config*);

  struct lokinet_config_iter;

  /// open an iterator to iterate over all config values in a lokinet config in the section `sect`
  struct lokinet_config_iter* EXPORT
  lokinet_config_iterate_section(struct lokinet_config*, const char* sect);

  /// fetch the next config value on this iterator.
  /// returns false if there are no more items left to iterate over.
  /// fills up opt_name and opt_val with option name and option value, or sets both to hold NULL if
  /// at the last entry.
  bool EXPORT
  lokinet_config_iter_next(
      struct lokinet_config_iter*, const char** opt_name, const char** opt_val);

  /// close a previously openend lokinet_config_iter
  void EXPORT
  lokinet_config_iter_free(struct lokinet_config_iter*);

  /// add to the lokinet config a config value for the section `sect` `opt_name=val`
  void EXPORT
  lokinet_config_add_opt(
      struct lokinet_config*, const char* sect, const char* opt_name, const char* val);

  /// write current config to an open file.
  /// returns number of bytes written or -1 on error.
  ssize_t EXPORT
  lokinet_config_to_file(struct lokinet_config*, FILE* f);

  /// helper function for reading config from file.
  static struct lokinet_config* EXPORT
  lokinet_config_read(const char* fname)
  {
    struct lokinet_config* conf;
    FILE* f;
    f = fopen(fname, "rb");
    if (f == NULL)
    {
      fprintf(stderr, "failed to open %s: %s", fname, strerror(errno));
      return NULL;
    }
    conf = lokinet_config_from_file(f);
    if (conf == NULL)
      fprintf(stderr, "failed to parse config file %s", fname);
    fclose(f);
    return conf;
  }

  /// helper function for writing config to file.
  static bool EXPORT
  lokinet_config_write(struct lokinet_config* conf, const char* fname)
  {
    ssize_t n;
    FILE* f;
    f = fopen(fname, "wb");
    if (f == NULL)
    {
      fprintf(stderr, "failed to open %s: %s", fname, strerror(errno));
      return NULL;
    }
    n = lokinet_config_to_file(conf, f);
    if (n == -1)
      fprintf(stderr, "failed to write config file %s", fname);
    fclose(f);
    return conf;
  }

#ifdef __cplusplus
}
#endif

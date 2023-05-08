#pragma once

#include "lokinet_config.h"
#include "lokinet_export.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

  struct lokinet_context;

  struct lokinet_config* EXPORT
  lokinet_context_new();

  /// allocate a new lokinet context, passing in a loaded config, finalizing the config's loaded
  /// values and injecting them into the lokinet context. if config is NULL we will load sensible
  /// defaults. after calling lokinet_context_new the config can be released by calling
  /// lokinet_config_free().
  struct lokinet_context* EXPORT
  lokinet_context_from_config(struct lokinet_config*);

  /// free a context allocated by lokinet_context_new
  void EXPORT
  lokinet_context_free(struct lokinet_context*);

  /// spawn all the threads needed for operation and start running
  /// return 0 on success
  /// return non zero on fail
  int EXPORT
  lokinet_context_start(struct lokinet_context*);

  /// return 0 if we our endpoint has published on the network and is ready to send
  /// return -1 if we don't have enough paths ready
  /// retrun -2 if we look deadlocked
  /// retrun -3 if context was null or not started yet
  int EXPORT
  lokinet_status(struct lokinet_context*);

  /// wait at most N milliseconds for lokinet to build paths and get ready
  /// return 0 if we are ready
  /// return nonzero if we are not ready
  int EXPORT
  lokinet_wait_for_ready(int N, struct lokinet_context*);

  /// stop all operations on this lokinet context
  void EXPORT
  lokinet_context_stop(struct lokinet_context*);

  /// load a bootstrap RC from memory
  /// return 0 on success
  /// return non zero on fail
  int EXPORT
  lokinet_add_bootstrap_rc(const char*, size_t, struct lokinet_context*);

#ifdef __cplusplus
}
#endif

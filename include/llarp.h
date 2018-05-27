#ifndef LLARP_H_
#define LLARP_H_
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/mem.h>
#include <llarp/nodedb.h>
#include <llarp/router.h>
#include <llarp/version.h>

#ifdef __cplusplus
extern "C" {
#endif

/** llarp application context for C api */
struct llarp_main;

/** initialize application context and load config */
bool
llarp_main_init(struct llarp_main **ptr, const char *fname);

/** handle signal for main context */
void
llarp_main_signal(struct llarp_main *ptr, int sig);

/** run main context */
int
llarp_main_run(struct llarp_main *ptr);

void
llarp_main_free(struct llarp_main **ptr);

#ifdef __cplusplus
}
#endif
#endif

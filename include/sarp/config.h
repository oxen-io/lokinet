#ifndef SARP_CONFIG_H_
#define SARP_CONFIG_H_
#include <sarp/mem.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sarp_config;

void sarp_new_config(struct sarp_config ** conf, struct sarp_alloc * mem);
void sarp_free_config(struct sarp_config **  conf);

int sarp_load_config(struct sarp_config * conf, const char * fname);

#ifdef __cplusplus
}
#endif
#endif

#ifndef SARP_CONFIG_H_
#define SARP_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct sarp_config;

void sarp_new_config(struct sarp_config ** conf);
void sarp_free_config(struct sarp_config **  conf);

  /** @brief return -1 on fail otherwiwse 0 */
int sarp_load_config(struct sarp_config * conf, const char * fname);

struct sarp_config_iterator
{
  void * user;
  /** set by sarp_config_iter */
  struct sarp_config * conf;
  /** visit (self, section, key, value) */
  void (*visit)(struct sarp_config_iterator *, const char *, const char *, const char *);
};
  
void sarp_config_iter(struct sarp_config * conf, struct sarp_config_iterator * iter);
  
#ifdef __cplusplus
}
#endif
#endif

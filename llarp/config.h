#ifndef LLARP_CONFIG_H_
#define LLARP_CONFIG_H_

#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * config.h
   *
   * library configuration utilties
   */

  struct llarp_config;

  /// allocate config
  void
  llarp_new_config(struct llarp_config **conf);

  /// deallocate config
  void
  llarp_free_config(struct llarp_config **conf);

  /// @brief return -1 on fail otherwiwse 0
  int
  llarp_load_config(struct llarp_config *conf, const char *fname);

  /// config iterator configuration
  struct llarp_config_iterator
  {
    /// a customizable pointer to pass data to iteration functor
    void *user;
    /// set by llarp_config_iter
    struct llarp_config *conf;
    /// visit (self, section, key, value)
    void (*visit)(struct llarp_config_iterator *, const char *, const char *,
                  const char *);
  };

  /// iterator over "conf" and call visit functor defined in "iter"
  void
  llarp_config_iter(struct llarp_config *conf,
                    struct llarp_config_iterator *iter);

#ifdef __cplusplus
}
#endif
#endif

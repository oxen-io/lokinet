#ifndef LOKINET_H
#define LOKINET_H

#ifdef __cplusplus
extern "C" {
#endif


  struct lokinet_context;

  struct lokinet_context *
  lokinet_context_new();

  void
  lokinet_context_free(struct lokinet_context *);
  
  


#ifdef __cplusplus
}
#endif
#endif

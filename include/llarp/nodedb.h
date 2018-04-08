#ifndef LLARP_NODEDB_H
#define LLARP_NODEDB_H
#include <llarp/common.h>
#include <llarp/crypto.h>
#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_nodedb;

  /** create an empty nodedb */
  struct llarp_nodedb * llarp_nodedb_new();

  /** free a nodedb and all loaded rc */
  void llarp_nodedb_free(struct llarp_nodedb ** n);

  /** ensure a nodedb fs skiplist structure is at dir
      create if not there.
   */
  bool llarp_nodedb_ensure_dir(const char * dir);
  
  /** load entire nodedb from fs skiplist at dir */
  ssize_t llarp_nodedb_load_dir(struct llarp_nodedb * n, const char * dir);

  /** store entire nodedb to fs skiplist at dir */
  ssize_t llarp_nodedb_store_dir(struct llarp_nodedb * n, const char * dir);

  struct llarp_nodedb_iter
  {
    void * user;
    struct llarp_rc * rc;
    bool (*visit)(struct llarp_nodedb_iter *);
  };

  /**
     iterate over all loaded rc with an iterator
   */
  void llarp_nodedb_iterate_all(struct llarp_nodedb * n, struct llarp_nodedb_iter i);
  
  /** 
      find rc by rc.k being value k
      returns true if found otherwise returns false
   */
  bool llarp_nodedb_find_rc(struct llarp_nodedb * n, struct llarp_rc * rc,  llarp_pubkey_t k);

  /**
     put an rc into the node db
     overwrites with new contents if already present
     flushes the single entry to disk
     returns true on success and false on error
   */
  bool llarp_nodedb_put_rc(struct llarp_nodedb * n, struct llarp_rc * rc);


  
#ifdef __cplusplus
}
#endif
#endif

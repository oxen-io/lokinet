#import <stdbool.h>

void
_enable_debug_mode();

bool
_llarp_ensure_config(const char *fname, const char *basedir, bool overwrite, bool asRouter);

struct llarp_main *
_llarp_main_init(const char *fname, bool multiProcess);

int
_llarp_main_setup(struct llarp_main *ptr);

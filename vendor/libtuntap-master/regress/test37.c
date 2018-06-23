/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

int exit_value;

void
test_cb(int level, const char *errmsg) {
	(void)level;
	(void)errmsg;
	fprintf(stderr, "successfully set a callback\n");
	exit_value = 0;
}

int
main(void) {
	struct device *dev;

	exit_value = 1;
	dev = tuntap_init();
	tuntap_log_set_cb(test_cb);

	tuntap_start(dev, 0, -1);

	tuntap_destroy(dev);
	return exit_value;
}


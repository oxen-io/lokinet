/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

/*
 * This test seems to pass on Windows, but it's not true.
 * tuntap_up() should be call _after_ tuntap_set_ip(), as in test17.c
 *
 * Fuck.
 */
int
main(void) {
	int ret;
	struct device *dev;

	ret = 0;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_up(dev) == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_set_ip(dev, "1.2.3.4", 24) == -1) {
		ret = 1;
	}

clean:
	tuntap_destroy(dev);
	return ret;
}


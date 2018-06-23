/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

int
main(void) {
	struct device *dev;

	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1) {
		return 1;
	}

	if (tuntap_set_ip(dev, NULL, 24) == -1) {
		tuntap_destroy(dev);
		return 0;
	}

	return 1;
}


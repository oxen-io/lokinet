/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

int
main(void) {
	int ret;
	struct device *dev;

	ret = 1;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1)
		ret = 1;

	if (tuntap_set_descr(dev, NULL) == -1) {
		ret = 0;
	}

	tuntap_destroy(dev);
	return ret;
}


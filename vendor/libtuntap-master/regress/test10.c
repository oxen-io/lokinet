/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

/* This test SHOULD fail, it's normal */

int
main(void) {
	int ret;
	struct device *dev;

	ret = 1;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY) == -1) {
		goto clean;
	}

	if (tuntap_set_hwaddr(dev, "random") == -1) {
		ret = 0;
	}

clean:
	tuntap_destroy(dev);
	return ret;
}


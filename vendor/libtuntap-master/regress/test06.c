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
	int mtu;
	struct device *dev;

	ret = 0;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY) == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_set_mtu(dev, 1400) == -1) {
		ret = 1;
		goto clean;
	}

	mtu = tuntap_get_mtu(dev);
	if (mtu != 1400) {
		ret = 1;
	}

clean:
	tuntap_destroy(dev);
	return ret;
}


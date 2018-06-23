/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif
#include <string.h>

#include "tuntap.h"

int
main(void) {
	int ret;
	struct device *dev;
	const char *s = "This tap interface is here for testing purpose";
	char *check_s;

	ret = 0;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_set_descr(dev, s) == -1) {
		ret = 1;
		goto clean;
	}

	if ((check_s = tuntap_get_descr(dev)) == NULL) {
		ret = 1;
		goto clean;
	}

	if (strcmp(s, check_s) != 0) {
		ret = 1;
	}

clean:
	tuntap_destroy(dev);
	return ret;
}


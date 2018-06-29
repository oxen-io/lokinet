/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

int
main(void) {
	int ret;
	struct device *dev;
	char *hwaddr;

	ret = 1;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1) {
		goto clean;
	}

	hwaddr = tuntap_get_hwaddr(dev);
	(void)fprintf(stderr, "%s\n", hwaddr);
	if (strcmp(hwaddr, "0:0:0:0:0:0") == 0)
		goto clean;
	if (strcmp(hwaddr, "00:00:00:00:00:00") == 0)
		goto clean;

	ret = 0;
clean:
	tuntap_destroy(dev);
	return ret;
}


/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
# define strcasecmp(x, y) _stricmp((x), (y))
#else
# include <strings.h>
#endif

#include "tuntap.h"

int
main(void) {
	int ret;
	char *hwaddr;
	struct device *dev;

	ret = 0;
	dev = tuntap_init();
	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY)
	    == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_set_hwaddr(dev, "54:1a:13:ef:b6:b5") == -1) {
		ret = 1;
		goto clean;
	}

	hwaddr = tuntap_get_hwaddr(dev);
	if (strcasecmp(hwaddr, "54:1a:13:ef:b6:b5") != 0)
		ret = 1;

clean:
	tuntap_destroy(dev);
	return ret;
}


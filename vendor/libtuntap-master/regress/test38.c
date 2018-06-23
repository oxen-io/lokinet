/* Public domain - Tristan Le Guern <tleguern@bouledef.eu> */

#include <sys/types.h>

#include <stdio.h>
#if defined Windows
# include <windows.h>
#endif

#include "tuntap.h"

int debug, info, notice, warn, err;

void
test_cb(int level, const char *errmsg) {
	const char *prefix = NULL;

	switch (level) {
	case TUNTAP_LOG_DEBUG:
		prefix = "debug";
		debug = 1;
		break;
	case TUNTAP_LOG_INFO:
		prefix = "info";
		info = 1;
		break;
	case TUNTAP_LOG_NOTICE:
		prefix = "notice";
		notice = 1;
		break;
	case TUNTAP_LOG_WARN:
		prefix = "warn";
		warn = 1;
		break;
	case TUNTAP_LOG_ERR:
		prefix = "err";
		err = 1;
		break;
	default:
		/* NOTREACHED */
		break;
	}
	(void)fprintf(stderr, "%s: %s\n", prefix, errmsg);
}

int
main(void) {
	tuntap_log_set_cb(test_cb);

	tuntap_log(TUNTAP_LOG_DEBUG, "debug message");
	tuntap_log(TUNTAP_LOG_INFO, "info message");
	tuntap_log(TUNTAP_LOG_NOTICE, "notice message");
	tuntap_log(TUNTAP_LOG_WARN, "warn message");
	tuntap_log(TUNTAP_LOG_ERR, "err message");

	if (debug + info + notice + warn + err != 5)
		return -1;
	return 0;
}


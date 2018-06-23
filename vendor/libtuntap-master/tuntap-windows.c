/*
 * Copyright (c) 2012 Tristan Le Guern <tleguern@bouledef.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <strsafe.h>

#include "tuntap.h"

/* From OpenVPN tap driver, common.h */
#define TAP_CONTROL_CODE(request,method) CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
#define TAP_IOCTL_GET_MAC               TAP_CONTROL_CODE (1, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION           TAP_CONTROL_CODE (2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU               TAP_CONTROL_CODE (3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO              TAP_CONTROL_CODE (4, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE (5, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS      TAP_CONTROL_CODE (6, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ      TAP_CONTROL_CODE (7, METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE          TAP_CONTROL_CODE (8, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT   TAP_CONTROL_CODE (9, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_TUN            TAP_CONTROL_CODE (10, METHOD_BUFFERED)

/* Windows registry crap */
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define NETWORK_ADAPTERS "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

/* From OpenVPN tap driver, proto.h */
typedef unsigned long IPADDR;

/* This one is from Fabien Pichot, in the tNETacle source code */
static LPWSTR
formated_error(LPWSTR pMessage, DWORD m, ...) {
    LPWSTR pBuffer = NULL;

    va_list args = NULL;
    va_start(args, pMessage);

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_ALLOCATE_BUFFER,
                  pMessage, 
                  m,
                  0,
                  (LPSTR)&pBuffer, 
                  0, 
                  &args);

    va_end(args);

    return pBuffer;
}

/* TODO: Rework to be more generic and allow arbitrary key modification (MTU and stuff) */
static char *
reg_query(char *key_name) {
	HKEY adapters, adapter;
	DWORD i, ret, len;
	char *deviceid = NULL;
	DWORD sub_keys = 0;

	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(key_name), 0, KEY_READ, &adapters);
	if (ret != ERROR_SUCCESS) {
		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", ret));
		return NULL;
	}

	ret = RegQueryInfoKey(adapters,	NULL, NULL, NULL, &sub_keys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (ret != ERROR_SUCCESS) {
		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", ret));
		return NULL;
	}

	if (sub_keys <= 0) {
		tuntap_log(TUNTAP_LOG_DEBUG, "Wrong registry key");
		return NULL;
	}

	/* Walk througt all adapters */
    for (i = 0; i < sub_keys; i++) {
		char new_key[MAX_KEY_LENGTH];
		char data[256];
		TCHAR key[MAX_KEY_LENGTH];
		DWORD keylen = MAX_KEY_LENGTH;

		/* Get the adapter key name */
		ret = RegEnumKeyEx(adapters, i, key, &keylen, NULL, NULL, NULL, NULL);
		if (ret != ERROR_SUCCESS) {
			continue;
		}
		
		/* Append it to NETWORK_ADAPTERS and open it */
		snprintf(new_key, sizeof new_key, "%s\\%s", key_name, key);
		ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(new_key), 0, KEY_READ, &adapter);
		if (ret != ERROR_SUCCESS) {
			continue;
		}

		/* Check its values */
		len = sizeof data;
		ret = RegQueryValueEx(adapter, "ComponentId", NULL, NULL, (LPBYTE)data, &len);
		if (ret != ERROR_SUCCESS) {
			/* This value doesn't exist in this adaptater tree */
			goto clean;
		}
		/* If its a tap adapter, its all good */
		if (strncmp(data, "tap", 3) == 0) {
			DWORD type;

			len = sizeof data;
			ret = RegQueryValueEx(adapter, "NetCfgInstanceId", NULL, &type, (LPBYTE)data, &len);
			if (ret != ERROR_SUCCESS) {
				tuntap_log(TUNTAP_LOG_INFO, (const char *)formated_error(L"%1", ret));
				goto clean;
			}
			deviceid = strdup(data);
			break;
		}
clean:
		RegCloseKey(adapter);
	}
	RegCloseKey(adapters);
	return deviceid;
}

void
tuntap_sys_destroy(struct device *dev) {
	(void)dev;
	return;
}

int
tuntap_start(struct device *dev, int mode, int tun) {
	HANDLE tun_fd;
	char *deviceid;
	char buf[60];

	/* Don't re-initialise a previously started device */
	if (dev->tun_fd != TUNFD_INVALID_VALUE) {
		return -1;
	}

	/* Shift the persistence bit */
	if (mode & TUNTAP_MODE_PERSIST) {
		mode &= ~TUNTAP_MODE_PERSIST; 
	}

	if (mode == TUNTAP_MODE_TUNNEL) {
		tuntap_log(TUNTAP_LOG_NOTICE, "Layer 3 tunneling is not implemented");
		return -1;
	}
	else if (mode != TUNTAP_MODE_ETHERNET) {
		tuntap_log(TUNTAP_LOG_ERR, "Invalid parameter 'mode'");
		return -1;
	}

	deviceid = reg_query(NETWORK_ADAPTERS);
	snprintf(buf, sizeof buf, "\\\\.\\Global\\%s.tap", deviceid);
	tun_fd = CreateFile(buf, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED, 0);
	if (tun_fd == TUNFD_INVALID_VALUE) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
	}

	dev->tun_fd = tun_fd;
	return 0;
}

void
tuntap_release(struct device *dev) {
	(void)CloseHandle(dev->tun_fd);
	free(dev);
}

char *
tuntap_get_hwaddr(struct device *dev) {
	static unsigned char hwaddr[ETHER_ADDR_LEN];
	DWORD len;

    if (DeviceIoControl(dev->tun_fd, TAP_IOCTL_GET_MAC, &hwaddr, sizeof(hwaddr), &hwaddr, sizeof(hwaddr), &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return NULL;
    } else {
		char buf[128];
	
		(void)_snprintf_s(buf, sizeof buf, sizeof buf, "MAC address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			hwaddr[0],hwaddr[1],hwaddr[2],hwaddr[3],hwaddr[4],hwaddr[5]);
		tuntap_log(TUNTAP_LOG_DEBUG, buf);
	}
	return (char *)hwaddr;
}

int
tuntap_set_hwaddr(struct device *dev, const char *hwaddr) {
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_hwaddr()");
	return -1;
}

static int
tuntap_sys_set_updown(struct device *dev, ULONG flag) {
	DWORD len;

	if (DeviceIoControl(dev->tun_fd, TAP_IOCTL_SET_MEDIA_STATUS, &flag, sizeof(flag), &flag, sizeof(flag), &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
    } else {
		char buf[32];

		(void)_snprintf_s(buf, sizeof buf, sizeof buf, "Status: %s", flag ? "Up" : "Down");
		tuntap_log(TUNTAP_LOG_DEBUG, buf);
	return 0;
	}
}

int
tuntap_up(struct device *dev) {
	ULONG flag;

	flag = 1;
	return tuntap_sys_set_updown(dev, flag);
}

int
tuntap_down(struct device *dev) {
	ULONG flag;

	flag = 0;
	return tuntap_sys_set_updown(dev, flag);
}

int
tuntap_get_mtu(struct device *dev) {
	ULONG mtu;
	DWORD len;

	if (DeviceIoControl(dev->tun_fd, TAP_IOCTL_GET_MTU, &mtu, sizeof(mtu), &mtu, sizeof(mtu), &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
    }
	return 0;
}

int
tuntap_set_mtu(struct device *dev, int mtu) {
	(void)dev;
	(void)mtu;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_mtu()");
	return -1;
}

int
tuntap_sys_set_ipv4(struct device *dev, t_tun_in_addr *s, uint32_t mask) {
	IPADDR psock[4];
	DWORD len;

	/* Address + Netmask */
	psock[0] = s->S_un.S_addr; 
	psock[1] = mask;
	/* DHCP server address (We don't want it) */
	psock[2] = 0;
	/* DHCP lease time */
	psock[3] = 0;

	if (DeviceIoControl(dev->tun_fd, TAP_IOCTL_CONFIG_DHCP_MASQ, &psock, sizeof(psock), &psock, sizeof(psock), &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
    }
	return 0;
}

int
tuntap_sys_set_ipv6(struct device *dev, t_tun_in6_addr *s, uint32_t mask) {
	(void)dev;
	(void)s;
	(void)mask;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_sys_set_ipv6()");
	return -1;
}

int
tuntap_read(struct device *dev, void *buf, size_t size) {
	DWORD len;

	if (ReadFile(dev->tun_fd, buf, (DWORD)size, &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
	}

	return 0;
}

int
tuntap_write(struct device *dev, void *buf, size_t size) {
	DWORD len;

	if (WriteFile(dev->tun_fd, buf, (DWORD)size, &len, NULL) == 0) {
		int errcode = GetLastError();

		tuntap_log(TUNTAP_LOG_ERR, (const char *)formated_error(L"%1%0", errcode));
		return -1;
	}

	return 0;
}

int
tuntap_get_readable(struct device *dev) {
	(void)dev;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_get_readable()");
	return -1;
}

int
tuntap_set_nonblocking(struct device *dev, int set) {
	(void)dev;
	(void)set;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_nonblocking()");
	return -1;
}

int
tuntap_set_debug(struct device *dev, int set) {
	(void)dev;
	(void)set;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_debug()");
	return -1;
}

int
tuntap_set_descr(struct device *dev, const char *descr) {
	(void)dev;
	(void)descr;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_descr()");
	return -1;
}

int
tuntap_set_ifname(struct device *dev, const char *name) {
	/* TODO: Check Windows API to know how to rename an interface */
	(void)dev;
	(void)name;
	tuntap_log(TUNTAP_LOG_NOTICE, "Your system does not support tuntap_set_ifname()");
	return -1;
}

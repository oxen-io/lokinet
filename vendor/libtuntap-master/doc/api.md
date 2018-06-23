API
===

Types
=====

struct device
-------------

The `struct device` is an opaque structure containing internal parameter, like the device name, device flags, device file descriptor, etc. You should not access them.

The structure size may vary from an operating system to an other, you should not rely on it.

t_tuntap_log
------------

    typedef void (*t_tuntap_log)(int level, const char *msg);

This type is a pointer to a log function. It allows to override the default behaviour, which is printing every messages on the error output prefixed with its error level.

Error levels are described later, they are macros in the form `TUNTAP_LOG_*`.

t_tun
-----

The `t_tun` type map the file descriptor type of a given operating system.

Typically it's an `int` on UNIXes and a `HANDLE` on Windows. 

Macros
======

There is two type of macros, the libtuntap ones, and the "portable" ones. The laters are here to help portable parts of the code to rely on meaningful names, not hard-coded magic values.

libtuntap macros
----------------

TUNTAP_ID_MAX
-------------

`TUNTAP_ID_MAX` is the maximal device unit giveable as the third parameter of `tuntap_start()`.

TUNTAP_ID_ANY
-------------

`TUNTAP_ID_ANY` is the wild-card device unit giveable as the third parameter of `tuntap_start()`.

TUNTAP_MODE_ETHERNET
--------------------

`TUNTAP_MODE_ETHERNET` is the tap-mode giveable as the second parameter of `tuntap_start()`.

TUNTAP_MODE_TUNNEL
------------------

`TUNTAP_MODE_TUNNEL` is the tun-mode giveable as the second parameter of `tuntap_start()`.

TUNTAP_MODE_PERSIST
-------------------

`TUNTAP_MODE_PERSIST` is the persistence flag giveable OR'ed with the second parameter of `tuntap_start()`.

This flag is optional and should be used with either `TUNTAP_MODE_TUNNEL` or `TUNTAP_MODE_ETHERNET`.

TUNTAP_LOG_ERR
--------------

`TUNTAP_LOG_ERR` describes an error message.

TUNTAP_LOG_WARN
---------------

`TUNTAP_LOG_WARN` describes a warning message.

TUNTAP_LOG_INFO
---------------

`TUNTAP_LOG_INFO` describes an informational message.

TUNTAP_LOG_NOTICE
-----------------

`TUNTAP_LOG_NOTICE` describes a message which is not really an error nor a warning. It is mostly used to warn about unimplemented or unavailable part of the libtuntap.

TUNTAP_LOG_DEBUG
----------------

`TUNTAP_LOG_DEBUG` describes a debug messages. You should see one only if your are using the git HEAD.

TUNTAP_LOG_NONE
---------------

`TUNTAP_LOG_NONE` describes other things. It is only used by `tuntap_log_hexdump()` and `tuntap_log_chksum()`.

Portable macros
---------------

ETHER_ADDR_LEN
--------------

`ETHER_ADDR_LEN` is a value dictated by [[http://www.ieee802.org/3/|IEEE 802.3]] standard.
On Linux systems its value is mapped on `ETH_ALEN` one.

IF_NAMESIZE
-----------

`IF_NAMESIZE` gives the maximal length of an interface name.
On BSD systems its value is mapped on `IFNAMSIZ` one.

IF_DESCRSIZE
------------

`IF_DESCSIZE` gives the maximal length of an interface description.

TUNFD_INVALID_VALUE
-------------------

`TUNFD_INVALID_VALUE` is the invalid value of the t_tun type.
On UNIXes systems its value is `-1`.
On Windows systems its value is `INVALID_HANDLE_VALUE`.

Functions
=========

tuntap_init
-----------

    struct device *tuntap_init(void);

This function will allocate and initialise a `struct device`.

tuntap_version
--------------

    int tuntap_version(void);

This function returns the version number of libtuntap.
You can extract the major and the minor like this:

    int version = tuntap_version();
    int major = version >> 8;
    int minor = version & major;

Note that this version number is not the same as the shared library version.

tuntap_destroy
--------------

    void tuntap_destroy(struct device *dev);

This function will free allocated memory, close file descriptors and destroy the interface.

=== tuntap_release

    void tuntap_release(struct device *dev);

This function will free allocated memory and close file descriptors. leaving the interface.

=== tuntap_start

    int tuntap_start(struct device dev*, int mode, int unit);

This function will configure the device with the mode `mode` and the optional device unit `unit`.

The `mode` parameter should be either `TUNTAP_MODE_ETHERNET` or `TUNTAP_MODE_TUNNEL` eventually ORed with `TUNTAP_MODE_PERSIST`.

The `unit` parameter should always be less than `TUNTAP_ID_MAX` and greater than 0. If you don't need a particular value, you should use `TUNTAP_ID_ANY`, which is a sort of wild-card device unit. With this parameter, the kernel will pick the next available device.
The term "device unit" is also known as PPA, for Physical Point of Attachment, in Solaris documentation.

Examples:

    tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY)

    tuntap_start(dev, TUNTAP_MODE_TUNNEL, 2)

    tuntap_start(dev, TUNTAP_MODE_TUNNEL | TUNTAP_MODE_PERSIST, TUNTAP_ID_ANY)

tuntap_get_ifname
-----------------

    char     *tuntap_get_ifname(struct device *dev);

This function fetch and return the name of the interface described by `dev`.

tuntap_set_ifname
-----------------

    int tuntap_set_ifname(struct device *dev, const char *ifname);

This function replaces the name of the interface described by `dev` with the given name `ifname`.

It returns -1 on error.

Compatibility: Linux.

tuntap_get_hwaddr
-----------------

    char	 *tuntap_get_hwaddr(struct device *dev);

This function fetch and returns the link-layer address (MAC) of the interface described by `dev`.

The returned string come from a statically allocated buffer, ans thus should be saved if needed for later use.

tuntap_set_hwaddr
-----------------

    int tuntap_set_hwaddr(struct device *dev, const char *mac_addr);

This function replaces the link-layer address of the interface described by `dev` with the given address `mac_addr`.

It returns -1 on error.

tuntap_set_descr
----------------

    int tuntap_set_descr(struct device *dev, const char *desc);

This function replaces the description of the interface described by `dev` with the given string `desc`.

Compatibility: OpenBSD, FreeBSD.

tuntap_up
---------

    int tuntap_up(struct device *dev);

This function set interface to the UP state, just like `ifconfig eth0 up` would do.

tuntap_down
-----------

    int tuntap_down(struct device *dev);

This function set interface to the DOWN state, just like `ifconfig eth0 down` would do.

tuntap_get_mtu
--------------

    int tuntap_get_mtu(struct device *dev);

This function fetch and returns the Maximum Transfer Unit (MTU) of the interface described by `dev`.

tuntap_set_mtu
--------------

    int tuntap_set_mtu(struct device *dev, int mtu);

This function replaces the MTU of the interface described by `dev` with the given value `mtu`.

tuntap_set_ip
-------------

    int tuntap_set_ip(struct device *dev, const char *, int ip_addr);

This function replaces the IP address of the interface described by `dev` with the given address `ip_addr`.

tuntap_read
-----------

    int tuntap_read(struct device *dev, void *buf, size_t buf_len);

This function will read one packet from the interface descibed by `dev` and will store it in the buffer `buf` of size `buf_len`. This value can be retrieved with a call to `tuntap_get_readable()`.

tuntap_write
------------

    int tuntap_write(struct device *dev, void *buf, size_t buf_len);

This function will write the packet stored in the buffer `buf` of size `buf_len` to the interface descibed by `dev`.

tuntap_get_readable
-------------------

    int tuntap_get_readable(struct device *dev);

This function will return the size of the next packet waiting in the buffer queue of the interface described by `dev`.
On Linux this function is the same as `tuntap_get_mtu()`, because the ioctl call `FIONREAD` is not supported on caracter devices.

tuntap_set_nonblocking
----------------------

    int tuntap_set_nonblocking(struct device *dev, int set);

This function will set the socket of the interface described by `dev` to a non-blocking state.

tuntap_set_debug
----------------

    int tuntap_set_debug(struct device *dev, int set);

This function will enable or disable the debug mode of the interface described by `dev`, depending of the value of `set`.

If `set` is 0, it will disable debug, and if it is 1 it will enable it.

The debug mode will add more output regarding the interface on the console.

This functionality depend on your operating system. It is enable by default on FreeBSD and NetBSD, but you might have to recompile your tun and tap drivers on Linux and OpenBSD to enable it.

tuntap_log_set_cb
-----------------

    void tuntap_log_set_cb(t_tuntap_log cb);

This function allow to set an external printing function, in order to erase the default behaviour.

tuntap_log_hexdump
------------------

    void tuntap_log_hexdump(void *, size_t);

This function is actualy not documented.

tuntap_log_chksum
-----------------

    void tuntap_log_chksum(void *, int);

This function is actualy not documented.

TUNTAP_GET_FD
-------------

    int TUNTAP_GET_FD(struct device *dev)

This macro will return the socket of the interface described by `dev`.


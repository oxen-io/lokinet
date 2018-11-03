# TUN/TAP driver v9 for Windows

in order to set up tunnels on Windows, you will need
to instal this driver.

* v9.9.2.3 is for Windows 2000/XP/2003 (NDIS 5.0-based)
* v9.21.2 is for Windows Vista/7/8.1 and 10 (NDIS 6.0, forward-compatible with NDIS 10.0)

to instal, extract the corresponding version of the driver for your
platform and run `%ARCH%/install_tap.cmd` in an elevated shell

to remove *ALL* virtual tunnel adapters, run `%ARCH%/del_tap.cmd` in an elevated shell. Use the
Device Manager snap-in to remove individual adapter instances.

Both are signed by OpenVPN Inc, and are available for 32- and 64-bit archs.

-despair86
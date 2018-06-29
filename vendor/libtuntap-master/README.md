libtuntap
=========

Description
-----------

libtuntap is a library for configuring TUN or TAP devices in a portable manner.

TUN and TAP are virtual networking devices which allow userland applications
to receive packets sent to it. The userland applications can also send their
own packets to the devices and they will be forwarded to the kernel.

This is useful for developping tunnels, private networks or virtualisation
systems.

Supported Features
------------------

   * Creation of TUN _and_ TAP devices;
   * Autodetection of available TUN or TAP devices;
   * Setting and getting the MAC address of the device;
   * Setting and getting the MTU of the device;
   * Setting the status of the device (up/down);
   * Setting the IPv4 address and netmask of the device;
   * Setting the persistence mode of the device;
   * Setting the name of the device (Linux only);
   * Setting the description of the device (OpenBSD and FreeBSD only).

Supported Systems
-----------------

   * OpenBSD;
   * Linux;
   * NetBSD;
   * Darwin.

Current Porting Efforts
-----------------------

   * Windows;
   * FreeBSD.

In the future
-------------

   * AIX;
   * Solaris.

License
-------

All the code is licensed under the ISC License.
It's free, not GPLed !

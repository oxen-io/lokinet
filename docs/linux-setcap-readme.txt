Lokinet needs certain capabilities to run to set up a virtual network interface and provide a DNS server. The preferred approach to using this is through the linux capabilities mechanism, which allows assigning limited capabilities without needing to run the entire process as root.

There are two main ways to do this:

1. If you are running lokinet via an init system such as systemd, you can specify the capabilities in the service file by adding:

CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_BIND_SERVICE

  into the [Service] section of the systemd service file. This will assign the necessary permissions when running the process and allow lokinet to work while running as a non-root user.

2.  You can set the capabilities on the binary by using the setcap program (if not available you may need to install libcap2-bin on Debian/Ubuntu-based systems) and running:

setcap cap_net_admin,cap_net_bind_service=+eip lokinet

    This grants the permissions whenever the lokinet binary is executed.

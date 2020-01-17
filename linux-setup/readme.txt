You'll need to setcap the lokinet to make sure you don't have to run it as root
On debian-based distros, you make sure you have setcap installed first: apt install libcap2-bin
and then you can:
setcap cap_net_admin,cap_net_bind_service=+eip lokinet

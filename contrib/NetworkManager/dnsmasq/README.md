Place in `/etc/NetworkManager/dnsmasq.d/lokinet.conf`.

To make use of this, first install dnsmasq.

Then enable NetworkManager dnsmasq backend by editing `/etc/NetworkManager/NetworkManager.conf` to something like:
```
[main]
dns=dnsmasq
```
If NetworkManager is currently running, restart it for changes to take effect:
```
sudo systemctl restart NetworkManager
```

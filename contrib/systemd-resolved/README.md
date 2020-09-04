To be put at `/usr/lib/systemd/resolved.conf.d/lokinet.conf` for distro use and `/etc/systemd/resolved.conf.d/lokinet.conf` for local admin use.

To make use of it:
```
sudo ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
sudo systemctl enable --now systemd-resolved
```

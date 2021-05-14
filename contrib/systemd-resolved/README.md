Lokinet now talks to systemd directly via sdbus to set up DNS, but in order for this to work the
user running lokinet (assumed `_lokinet` in these example files) needs permission to set dns servers
and domains.

To set up the permissions:

- If lokinet is running as some user other than `_lokinet` the change the `_lokinet` username inside
  `lokinet.rules` and `lokinet.pkla`.

- If on a Debian or Debian-derived distribution (such as Ubuntu) using polkit 105,
  copy `lokinet.pkla` to `/var/lib/polkit-1/localauthority/10-vendor.d/lokinet.pkla` (for a distro
  install) or `/etc/polkit-1/localauthority.conf.d/` (for a local install).

- Copy `lokinet.rules` to `/usr/share/polkit-1/rules.d/` (distro install) or `/etc/polkit-1/rules.d`
  (local install).

Make use of it by switching to systemd-resolved:
```
sudo ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
sudo systemctl enable --now systemd-resolved
```

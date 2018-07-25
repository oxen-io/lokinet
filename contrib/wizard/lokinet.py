#!/usr/bin/env python3
from configparser import ConfigParser as Config 
import netifaces
import ipaddress
import os

def yield_public_addresses():
  for ifname in netifaces.interfaces():
    addrs = netifaces.ifaddresses(ifname)
    if netifaces.AF_INET in addrs:
      for addr in addrs[netifaces.AF_INET]:
        ip = addr['addr']
        if not ipaddress.ip_address(ip).is_private:
            yield ifname, ip

def genconf(rootdir):
  conf = Config()
  conf['router'] = {
    'threads': '2',
    'net-threads': '1',
    'contact-file': os.path.join(rootdir, 'self.signed'),
    'transport-privkey': os.path.join(rootdir, 'transport.key'),
    'identity-privkey': os.path.join(rootdir, 'identity.key')
  }
  conf['netdb'] = {
    'dir': os.path.join(rootdir, 'netdb')
  }
  conf['bind'] = {}
  found = False
  for ifname, ip in yield_public_addresses():
    conf['bind'][ifname] = '1090'
    print("using public address {}".format(ip))
    return conf
  else:
    print("This machine has no public network addresses")

def main():
  conf = genconf(os.path.realpath('.'))
  if conf:
    with open('daemon.ini', 'w') as f:
      conf.write(f)

if __name__ == '__main__':
  main()


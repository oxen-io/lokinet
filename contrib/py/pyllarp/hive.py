#!/usr/bin/env python3
import pyllarp
from time import sleep

def main():

  hive = pyllarp.RouterHive()
  config = pyllarp.Config()
  config.router.netid = "gamma"
  config.netdb.nodedbDir = "/home/tom/.lokinet/netdb"
  hive.AddRouter(config)
  hive.StartAll()
  sleep(10)
  hive.StopAll()

if __name__ == '__main__':
  main()

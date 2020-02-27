#!/usr/bin/env python3
import pyllarp

def main(conf):
  hive = pyllarp.RouterHive()
  config = pyllarp.Config()
  print("loading config: {}".format(conf))
  if not config.LoadFile(conf):
    print("failed to load {}".format(conf))
    return
  hive.AddRouter(config)
  hive.StartAll()
  hive.StopAll()

if __name__ == '__main__':
  import sys
  main(sys.argv[1])
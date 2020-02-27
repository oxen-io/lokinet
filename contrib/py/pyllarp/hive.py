#!/usr/bin/env python3
import pyllarp

def main():
  hive = pyllarp.RouterHive()
  config = pyllarp.Config()
  hive.AddRouter(config)
  hive.StartAll()
  hive.StopAll()

if __name__ == '__main__':
  main()
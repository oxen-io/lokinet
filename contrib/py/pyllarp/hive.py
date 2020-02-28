#!/usr/bin/env python3
import pyllarp
from time import sleep
from signal import signal, SIGINT

def main():

  running = True

  def handle_sigint(sig, frame):
    nonlocal running
    running = False

  signal(SIGINT, handle_signal)

  hive = pyllarp.RouterHive()
  config = pyllarp.Config()
  config.router.netid = "gamma"
  config.netdb.nodedbDir = "/home/tom/.lokinet/netdb"
  hive.AddRouter(config)
  hive.StartAll()

  while running:
    event = hive.GetNextEvent()
    print(event.ToString())

  hive.StopAll()

if __name__ == '__main__':
  main()

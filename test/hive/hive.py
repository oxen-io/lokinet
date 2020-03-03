#!/usr/bin/env python3
import pyllarp
from time import sleep
from signal import signal, SIGINT
from shutil import rmtree
from os import makedirs
from socket import AF_INET, htons, inet_aton
from pprint import pprint
import sys
from argparse import ArgumentParser as ap
import threading
from collections import deque

class RouterHive(object):

  def __init__(self, n_relays=10, n_clients=10, netid="hive"):

    self.endpointName = "pyllarp"
    self.tmpdir = "/tmp/lokinet_hive"
    self.netid = netid

    self.n_relays = n_relays
    self.n_clients = n_clients

    self.addrs = []
    self.events = deque()

    self.hive = None

    pyllarp.EnableDebug()
    if not self.RemoveTmpDir():
      raise RuntimeError("Failed to initialize Router Hive")

  def RemoveTmpDir(self):
    if self.tmpdir.startswith("/tmp/") and len(self.tmpdir) > 5:
      print("calling rmdir -r %s" % self.tmpdir)
      rmtree(self.tmpdir, ignore_errors=True)
      return True
    else:
      print("not removing dir %s because it doesn't start with /tmp/" % self.tmpdir)

    return False

  def MakeEndpoint(self, router, after):
    if router.IsRelay():
      return
    ep = pyllarp.Endpoint(self.endpointName, router)
    router.AddEndpoint(ep)
    if after is not None:
      router.CallSafe(lambda : after(ep))

  def AddRelay(self, index):
    dirname = "%s/relays/%d" % (self.tmpdir, index)
    makedirs("%s/netdb" % dirname, exist_ok=True)

    config = pyllarp.Config()

    port = index + 30000
    tunname = "lokihive%d" % index

    config.router.encryptionKeyfile = "%s/encryption.key" % dirname
    config.router.transportKeyfile = "%s/transport.key" % dirname
    config.router.identKeyfile = "%s/identity.key" % dirname
    config.router.ourRcFile = "%s/rc.signed" % dirname
    config.router.netid = self.netid
    config.router.nickname = "Router%d" % index
    config.router.publicOverride = True
    config.router.overrideAddress("127.0.0.1", '{}'.format(port))
    """
    config.router.ip4addr.sin_family = AF_INET
    config.router.ip4addr.sin_port = htons(port)
    config.router.ip4addr.sin_addr.set("127.0.0.1")
    """
    config.router.blockBogons = False

    config.network.enableProfiling = False
    config.network.routerProfilesFile = "%s/profiles.dat" % dirname
    config.network.netConfig = {"type": "null"}

    config.netdb.nodedbDir = "%s/netdb" % dirname

    config.links.InboundLinks = [("lo", AF_INET, port, set())]

    config.system.pidfile = "%s/lokinet.pid" % dirname

    config.dns.netConfig = {"local-dns": ("127.3.2.1:%d" % port)}

    if index != 1:
      config.bootstrap.routers = ["%s/relays/1/rc.signed" % self.tmpdir]

    self.hive.AddRelay(config)


  def AddClient(self, index):
    dirname = "%s/clients/%d" % (self.tmpdir, index)
    makedirs("%s/netdb" % dirname, exist_ok=True)

    config = pyllarp.Config()

    port = index + 40000
    tunname = "lokihive%d" % index

    config.router.encryptionKeyfile = "%s/encryption.key" % dirname
    config.router.transportKeyfile = "%s/transport.key" % dirname
    config.router.identKeyfile = "%s/identity.key" % dirname
    config.router.ourRcFile = "%s/rc.signed" % dirname
    config.router.netid = self.netid
    config.router.blockBogons = False

    config.network.enableProfiling = False
    config.network.routerProfilesFile = "%s/profiles.dat" % dirname
    config.network.netConfig = {"type": "null"}

    config.netdb.nodedbDir = "%s/netdb" % dirname

    config.system.pidfile = "%s/lokinet.pid" % dirname

    config.dns.netConfig = {"local-dns": ("127.3.2.1:%d" % port)}

    config.bootstrap.routers = ["%s/relays/1/rc.signed" % self.tmpdir]

    self.hive.AddClient(config)

  def onGotEndpoint(self, ep):
    addr = ep.OurAddress()
    self.addrs.append(pyllarp.ServiceAddress(addr))

  def sendToAddress(self, router, toaddr, pkt):
    if router.IsRelay():
      return
    if router.TrySendPacket("default", toaddr, pkt):
      print("sending {} bytes to {}".format(len(pkt), toaddr))

  def broadcastTo(self, addr, pkt):
    self.hive.ForEachRouter(lambda r : sendToAddress(r, addr, pkt))

  def InitFirstRC(self):
    print("Starting first router to init its RC for bootstrap")
    self.hive = pyllarp.RouterHive()
    self.AddRelay(1)
    self.hive.StartRelays()
    print("sleeping 2 sec to give plenty of time to save bootstrap rc")
    sleep(2)

    self.hive.StopAll()

  def Start(self):

    self.InitFirstRC()

    print("Resetting hive.  Creating %d relays and %d clients" % (self.n_relays, self.n_clients))

    self.hive = pyllarp.RouterHive()

    for i in range(1, self.n_relays + 1):
      self.AddRelay(i)

    for i in range(1, self.n_clients + 1):
      self.AddClient(i)

    print("Starting relays")
    self.hive.StartRelays()

    sleep(0.2)
    self.hive.ForEachRelay(lambda r: self.MakeEndpoint(r, self.onGotEndpoint))

    print("Sleeping 1 seconds before starting clients")
    sleep(1)

    self.hive.StartClients()

    sleep(0.2)
    self.hive.ForEachClient(lambda r: self.MakeEndpoint(r, self.onGotEndpoint))

  def Stop(self):
    self.hive.StopAll()

  def CollectNextEvent(self):
    self.events.append(self.hive.GetNextEvent())

  def CollectAllEvents(self):
    self.events.extend(self.hive.GetAllEvents())

  def PopEvent(self):
    self.CollectAllEvents()
    if len(self.events):
      return self.events.popleft()
    return None

def main(n_relays=10, n_clients=10, print_each_event=True):

  running = True

  def handle_sigint(sig, frame):
    nonlocal running
    running = False
    print("SIGINT received, attempting to stop all routers")

  signal(SIGINT, handle_sigint)

  try:
    hive = RouterHive(n_relays, n_clients)
    hive.Start()

  except Exception as err:
    print(err)
    return 1

  total_events = 0
  event_counts = dict()
  while running:
    event = hive.PopEvent()
    event_name = event.__class__.__name__
    if event:
      if print_each_event:
        print("Event: %s -- Triggered: %s" % (event_name, event.triggered))
        print(event)
        hops = getattr(event, "hops", None)
        if hops:
          for hop in hops:
            print(hop)

      total_events = total_events + 1
      if event_name in event_counts:
        event_counts[event_name] = event_counts[event_name] + 1
      else:
        event_counts[event_name] = 1

      if total_events % 10 == 0:
        pprint(event_counts)

    sleep(.01)

  print('stopping')
  hive.Stop()
  print('stopped')
  del hive

if __name__ == '__main__':
  parser = ap()
  print_events = False
  parser.add_argument('--print-events', dest="print_events", action='store_true')
  args = parser.parse_args()
  main(n_relays=30, print_each_event = args.print_events)

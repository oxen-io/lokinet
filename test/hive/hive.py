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
import traceback

class RouterHive(object):

  def __init__(self, n_relays=10, n_clients=10, netid="hive", shutup=True):
    self._log = pyllarp.LogContext()
    self._log.shutup = shutup
    try:

      self.endpointName = "pyllarp"
      self.tmpdir = "/tmp/lokinet_hive"
      self.netid = netid

      self.n_relays = n_relays
      self.n_clients = n_clients

      self.addrs = []
      self.events = deque()

      self.hive = None
      self.RCs = []

      pyllarp.EnableDebug()
      if not self.RemoveTmpDir():
        raise RuntimeError("Failed to initialize Router Hive")

    except Exception as error:
      print("Exception in __init__: ", error);

  def RemoveTmpDir(self):
    if self.tmpdir.startswith("/tmp/") and len(self.tmpdir) > 5:
      print("calling rmdir -r %s" % self.tmpdir)
      rmtree(self.tmpdir, ignore_errors=True)
      return True
    else:
      print("not removing dir %s because it doesn't start with /tmp/" % self.tmpdir)

    return False
    
  def AddRelay(self, index):
    dirname = "%s/relays/%d" % (self.tmpdir, index)
    makedirs("%s/nodedb" % dirname, exist_ok=True)

    config = pyllarp.Config(dirname)
    config.Load(None, True)

    port = index + 30000
    tunname = "lokihive%d" % index

    config.router.dataDir = dirname
    config.router.netid = self.netid
    config.router.nickname = "Router%d" % index
    config.router.overrideAddress('127.0.0.1:{}'.format(port))
    config.router.blockBogons = False

    config.network.enableProfiling = False
    config.network.endpointType = 'null'

    config.links.addInboundLink("lo", AF_INET, port)
    config.links.setOutboundLink("lo", AF_INET, 0)

    # config.dns.options = {"local-dns": ("127.3.2.1:%d" % port)}
    if index == 0:
      config.bootstrap.seednode = True
    else:
      config.bootstrap.routers = ["%s/relays/0/self.signed" % self.tmpdir]

    config.api.enableRPCServer = False

    config.lokid.whitelistRouters = False
    print("adding relay at index %d" % index)
    self.hive.AddRelay(config)

  def AddClient(self, index):
    dirname = "%s/clients/%d" % (self.tmpdir, index)
    makedirs("%s/nodedb" % dirname, exist_ok=True)

    config = pyllarp.Config(dirname)
    config.Load(None, False);

    tunname = "lokihive%d" % index

    config.paths.netmask = 0
    config.router.dataDir = dirname
    config.router.netid = self.netid
    config.router.blockBogons = False

    config.network.enableProfiling = False
    config.network.endpointType = 'null'

    config.links.setOutboundLink("lo", AF_INET, 0);

    # config.dns.options = {"local-dns": ("127.3.2.1:%d" % port)}

    config.bootstrap.routers = ["%s/relays/0/self.signed" % self.tmpdir]

    config.api.enableRPCServer = False

    config.lokid.whitelistRouters = False

    self.hive.AddClient(config)

  def InitFirstRC(self):
    print("Starting first router to init its RC for bootstrap")
    self.hive = pyllarp.RouterHive()
    self.AddRelay(0)
    self.hive.StartRelays()
    print("sleeping 2 sec to give plenty of time to save bootstrap rc")
    sleep(2)

    self.hive.StopAll()

  def Start(self):

    self.InitFirstRC()

    print("Resetting hive.  Creating %d relays and %d clients" % (self.n_relays, self.n_clients))

    self.hive = pyllarp.RouterHive()

    for i in range(0, self.n_relays):
      self.AddRelay(i)

    for i in range(0, self.n_clients):
      self.AddClient(i)

    print("Starting relays")
    self.hive.StartRelays()

    print("Sleeping 2 seconds before starting clients")
    sleep(2)

    self.RCs = self.hive.GetRelayRCs()

    self.hive.StartClients()

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

  def DistanceSortedRCs(self, dht_key):
    rcs = []
    distances = []
    for rc in self.RCs:
      distances.append(rc.AsDHTKey ^ dht_key)
      rcs.append(rc)

    distances, rcs = (list(t) for t in zip(*sorted(zip(distances, rcs))))
    return rcs


def main(n_relays=10, n_clients=10, print_each_event=True, verbose=False):

  running = True

  def handle_sigint(sig, frame):
    nonlocal running
    running = False
    print("SIGINT received, attempting to stop all routers")

  signal(SIGINT, handle_sigint)

  try:
    hive = RouterHive(n_relays, n_clients, shutup=not verbose)
    hive.Start()

  except Exception as err:
    print(err)
    return 1

  first_dht_pub = False
  dht_pub_sorted = None
  dht_pub_location = None
  total_events = 0
  event_counts = dict()
  while running:
    hive.CollectAllEvents()
    print("Hive collected {} events this second.".format(len(hive.events)))
    for event in hive.events:
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

        if event_name == "DhtPubIntroReceivedEvent":
          if not first_dht_pub:
            dht_pub_sorted = hive.DistanceSortedRCs(event.location)
            dht_pub_location = event.location
            print("location: {} landed at: {}, sorted distance list:".format(dht_pub_location.ShortString(), event.routerID.ShortString()))
            print([x.routerID.ShortString() for x in dht_pub_sorted[:4]])
            first_dht_pub = True
          else:
            if event.location == dht_pub_location:
              print("location: {}, landed at: {}".format(dht_pub_location.ShortString(), event.routerID.ShortString()))

    # won't have printed event count above in this case.
    if len(hive.events) == 0:
      pprint(event_counts)

    hive.events = []
    for _ in range(100):
      sleep(1.0 / 100)

  print('stopping')
  hive.Stop()
  print('stopped')
  del hive

if __name__ == '__main__':
  parser = ap()
  print_events = False
  relay_count = 10
  client_count = 10
  parser.add_argument('--print-events', dest="print_events", action='store_true')
  parser.add_argument('--relay-count', dest="relay_count", type=int, default=10)
  parser.add_argument('--client-count', dest="client_count", type=int, default=10)
  parser.add_argument('--verbose', action='store_true', dest='verbose')
  args = parser.parse_args()
  main(n_relays=args.relay_count, n_clients=args.client_count, print_each_event = args.print_events, verbose=args.verbose)

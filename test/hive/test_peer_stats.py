import pyllarp
from time import time

def test_peer_stats(HiveForPeerStats):

  numRelays = 12

  hive = HiveForPeerStats(n_relays=numRelays, n_clients=0, netid="hive")

  start_time = time()
  cur_time = start_time
  test_duration = 30 #seconds

  # we track the number of attempted sessions and inbound/outbound established sessions
  numInbound = 0
  numOutbound = 0
  numAttempts = 0

  # we pick an arbitrary router out of our routers
  someRouterId = None

  while cur_time < start_time + test_duration:

    hive.CollectAllEvents()

    for event in hive.events:
      event_name = event.__class__.__name__

      if event_name == "LinkSessionEstablishedEvent":
        if event.inbound:
          numInbound += 1
        else:
          numOutbound += 1

      if event_name == "ConnectionAttemptEvent":
        numAttempts += 1

        if someRouterId is None:
          someRouterId = event.remoteId;

    hive.events = []
    cur_time = time()

  # these should be strictly equal, although there is variation because of
  # the time we sample
  print("test duration exceeded")
  print("in: {} out: {} attempts: {}", numInbound, numOutbound, numAttempts);
  totalReceived = tally_rc_received_for_peer(hive.hive, someRouterId)

  # every router should have received this relay's RC exactly once
  print("total times RC received: {} numRelays: {}", totalReceived, numRelays)
  assert(totalReceived == numRelays)

def tally_rc_received_for_peer(hive, routerId):

  numFound = 0

  def visit(relay):
    nonlocal numFound

    peerDb = relay.peerDb()
    stats = peerDb.getCurrentPeerStats(routerId);

    assert(stats.routerId == routerId)

    numFound += stats.numDistinctRCsReceived

  hive.ForEachRelayRouter(visit)

  return numFound;


if __name__ == "__main__":
  main()

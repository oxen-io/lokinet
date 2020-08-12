import pyllarp
from time import time

def test_peer_stats(HiveForPeerStats):
  return
  numRelays = 12

  hive = HiveForPeerStats(n_relays=numRelays, n_clients=0, netid="hive")
  someRouterId = None

  def collectStatsForAWhile(duration):
    print("collecting router hive stats for {} seconds...", duration)

    start_time = time()
    cur_time = start_time

    # we track the number of attempted sessions and inbound/outbound established sessions
    numInbound = 0
    numOutbound = 0
    numAttempts = 0

    nonlocal someRouterId

    while cur_time < start_time + duration:
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

          # we pick an arbitrary router out of our routers
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

    return {
      "numInbound": numInbound,
      "numOutbound": numOutbound,
      "numAttempts": numAttempts,
      "totalTargetRCsReceived": totalReceived,
    };

  results1 = collectStatsForAWhile(30);
  assert(results1["totalTargetRCsReceived"] == numRelays)

  # stop our router from gossiping
  router = hive.hive.GetRelay(someRouterId, True)
  router.disableGossiping();

  ignore = collectStatsForAWhile(30);

  # ensure that no one hears a fresh RC from this router again
  print("Starting second (longer) stats collection...")
  results2 = collectStatsForAWhile(3600);
  assert(results2["totalTargetRCsReceived"] == numRelays) # should not have increased

def tally_rc_received_for_peer(hive, routerId):

  numFound = 0

  def visit(context):
    nonlocal numFound

    peerDb = context.getRouterAsHiveRouter().peerDb()
    stats = peerDb.getCurrentPeerStats(routerId);

    assert(stats.routerId == routerId)

    numFound += stats.numDistinctRCsReceived

  hive.ForEachRelay(visit)

  return numFound;


if __name__ == "__main__":
  main()

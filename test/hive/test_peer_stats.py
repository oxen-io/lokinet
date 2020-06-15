import pyllarp
from time import time

def test_peer_stats(HiveForPeerStats):
  hive = HiveForPeerStats(n_relays=12, n_clients=0, netid="hive")

  start_time = time()
  cur_time = start_time
  test_duration = 30 #seconds

  paths = []

  print("looking for events...")

  numInbound = 0
  numOutbound = 0
  numAttempts = 0

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

    hive.events = []
    cur_time = time()

  print("test duration exceeded")
  print("in: {} out: {} attempts: {}", numInbound, numOutbound, numAttempts);
  assert(numInbound == numOutbound)
  assert(numOutbound == numAttempts)

if __name__ == "__main__":
  main()

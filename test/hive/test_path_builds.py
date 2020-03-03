from time import time

def test_path_builds(HiveTenTen):
  h = HiveTenTen

  start_time = time()
  cur_time = start_time
  test_duration = 10 #seconds

  log_attempts = True

  paths = []

  while cur_time < start_time + test_duration:

    h.CollectAllEvents()

    for event in h.events:
      event_name = event.__class__.__name__

      if log_attempts and event_name == "PathAttemptEvent":
        path = dict()
        path["hops"] = event.hops
        path["received"] = [False] * len(event.hops)
        path["prev"] = [None] * len(event.hops)
        for i in range(1, len(event.hops)):
          path["prev"][i] = event.hops[i-1].rc.routerID
        path["prev"][0] = event.routerID
        paths.append(path)

      elif event_name == "PathRequestReceivedEvent":
        for path in paths:
          for i in range(len(path["hops"])):
            assert type(path["hops"][i].upstreamRouter) == type(event.nextHop)
            assert type(path["prev"][i]) == type(event.prevHop)
            assert type(path["hops"][i].txid) == type(event.txid)
            assert type(path["hops"][i].rxid) == type(event.rxid)
            if (path["hops"][i].upstreamRouter == event.nextHop and
                path["prev"][i] == event.prevHop and
                path["hops"][i].txid == event.txid and
                path["hops"][i].rxid == event.rxid):
              path["received"][i] = True

    h.events = []
    cur_time = time()

    # only collect path attempts for 3 seconds
    if cur_time > start_time + 3:
      log_attempts = False

  assert len(paths) > 0

  fail_count = 0
  expected_count = 0

  paths_ok = []
  for path in paths:
    path_ok = True
    for rcv in path["received"]:
      expected_count = expected_count + 1
      if not rcv:
        path_ok = False
        fail_count = fail_count + 1

    paths_ok.append(path_ok)

  print("Path count: {}, Expected rcv: {}, Failed rcv: {}".format(len(paths), expected_count, fail_count))

  for path_ok in paths_ok:
    assert path_ok


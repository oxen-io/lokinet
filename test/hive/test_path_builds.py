from time import time

def test_path_builds(HiveArbitrary):
  h = HiveArbitrary(n_relays=30, n_clients=10)

  start_time = time()
  cur_time = start_time
  test_duration = 5 #seconds

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
        path["rxid"] = event.hops[0].rxid
        path["status"] = None
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

      elif event_name == "PathStatusReceivedEvent":
        for path in paths:
          if event.rxid == path["rxid"]:
            path["status"] = event

    h.events = []
    cur_time = time()

    # only collect path attempts for 3 seconds
    if cur_time > start_time + 3:
      log_attempts = False

  assert len(paths) > 0

  fail_status_count = 0
  missing_status_count = 0
  missing_rcv_count = 0
  expected_count = 0

  for path in paths:
    if path["status"]:
      if not path["status"].Successful:
        print(path["status"])
        fail_status_count = fail_status_count + 1
    else:
      missing_status_count = missing_status_count + 1

    for rcv in path["received"]:
      expected_count = expected_count + 1
      if not rcv:
        missing_rcv_count = missing_rcv_count + 1


  print("Path count: {}, Expected rcv: {}, missing rcv: {}, fail_status_count: {}, missing_status_count: {}".format(len(paths), expected_count, missing_rcv_count, fail_status_count, missing_status_count))

  assert fail_status_count == 0
  assert missing_rcv_count == 0
  assert missing_status_count == 0


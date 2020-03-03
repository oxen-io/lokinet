#!/usr/bin/env python3
import hive
import pytest

@pytest.fixture(scope="session")
def HiveTenTen():
  router_hive = hive.RouterHive(n_relays=10, n_clients=10, netid="hive")
  router_hive.Start()

  yield router_hive

  router_hive.Stop()

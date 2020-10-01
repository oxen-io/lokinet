#!/usr/bin/env python3
import hive
import pytest

@pytest.fixture(scope="session")
def HiveTenRTenC():
  router_hive = hive.RouterHive(n_relays=10, n_clients=10, netid="hive")
  router_hive.Start()

  yield router_hive

  router_hive.Stop()

@pytest.fixture(scope="session")
def HiveThirtyRTenC():
  router_hive = hive.RouterHive(n_relays=30, n_clients=10, netid="hive")
  router_hive.Start()

  yield router_hive

  router_hive.Stop()

@pytest.fixture()
def HiveArbitrary():
  router_hive = None
  def _make(n_relays=10, n_clients=10, netid="hive"):
    nonlocal router_hive
    router_hive = hive.RouterHive(n_relays=30, n_clients=10, netid="hive")
    router_hive.Start()
    return router_hive

  yield _make
  if router_hive:
    router_hive.Stop()

@pytest.fixture()
def HiveForPeerStats():
  router_hive = None
  def _make(n_relays, n_clients, netid):
    nonlocal router_hive
    router_hive = hive.RouterHive(n_relays, n_clients, netid)
    router_hive.Start()
    return router_hive

  yield _make
  if router_hive:
    router_hive.Stop()

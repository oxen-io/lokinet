#!/usr/bin/env python3
from util import run_lokinet


def test_lokinet_is_fast_af():
    assert run_lokinet(5)

def test_lokinet_is_fast():
    assert run_lokinet(10)

def test_lokinet_is_pretty_okay_speed():
    assert run_lokinet(30)

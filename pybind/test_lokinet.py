#!/usr/bin/env python3
from util import lokinet_run


def test_lokinet_fast_af():
    lokinet_run(5)
    
def test_lokinet_kinda_fast():
    lokinet_run(10)



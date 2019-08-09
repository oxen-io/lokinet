#!/usr/bin/env python3
import requests
import json
import time
import curses

class Canvas():

  def __init__(self):
    self.data = dict()
    self.win = curses.initscr()

  def __del__(self):
    self.win.clear()
    curses.endwin()

  def on_timer(self, event):
    """called on timer event"""
    self.update_data()

  def _log(self, *args):
    print("{}".format(*args))

  def jsonrpc(self, meth, params, url="http://127.0.0.1:1190/jsonrpc"):
    r = requests.post(url, headers={"Content-Type": "application/json"}, json={"jsonrpc": "2.0", "id": '0', "method":'{}'.format(meth), "params": params})
    return r.json()

  def update_data(self):
    """update data from lokinet"""
    try:
      j = self.jsonrpc("llarp.admin.dumpstate", {})
      self.data = j['result']
    except:
      self.data = None

  def _render_path(self,y, path, name):
    """render a path at current position"""
    self.win.move(y, 1)
    self.win.addstr("({}) ".format(name))
    y += 1
    self.win.move(y, 1)
    y += 1
    self.win.addstr("me -> ")
    for hop in path["hops"]:
      self.win.addstr(" {} ->".format(hop['router'][:4]))
    self.win.addstr(" [{} ms latency]".format(path['intro']['latency']))
    self.win.addstr(" [{} until expire]".format(self.timeTo(path["expiresAt"])))
    if path["expiresSoon"]:
      self.win.addstr("(expiring)")
    elif path["expired"]:
      self.win.addstr("(expired)")
    return y

  def timeTo(self, ts):
    now = time.time() * 1000
    return '{} seconds'.format(int((ts - now) / 1000))

  def display_service(self, y, name, status):
    """display a service at current position"""
    self.win.move(y, 1)
    self.win.addstr("service [{}]".format(name))
    build = status["buildStats"]
    ratio = build["success"] / ( build["attempts"] or 1)
    y += 1
    self.win.move(y, 1)
    self.win.addstr("build success: {} %".format(int(100 * ratio)))
    y += 1
    self.win.move(y, 1)
    paths = status["paths"]
    self.win.addstr("paths: {}".format(len(paths)))
    for path in paths:
      y = self._render_path(y, path, "inbound")
    for session in status["remoteSessions"]:
      for path in session["paths"]:
        y = self._render_path(y, path, "[active] {}".format(session["currentConvoTag"]))
    for session in status["snodeSessions"]:
      for path in session["paths"]:
        y = self._render_path(y, path, "[snode]")
    return y

    #for k in status:
    #  self.win.move(y + 1, 1)
    #  y += 1
    #  self.win.addstr('{}: {}'.format(k, json.dumps(status[k])))
      
  def display_links(self, y, data):
    for link in data["outbound"]:
      y += 1
      self.win.move(y, 1)
      self.win.addstr("outbound sessions:")
      y = self.display_link(y, link)
    for link in data["inbound"]:
      y += 1
      self.win.move(y, 1)
      self.win.addstr("inbound sessions:")
      y = self.display_link(y, link)
    return y

  def display_link(self, y, link):
    y += 1
    self.win.move(y, 1)
    sessions = link["sessions"]["established"]
    for s in sessions:
      y += 1
      self.win.move(y, 1)
      self.win.addstr('{}\t[{}\ttx]\t[{}\trx]'.format(s['remoteAddr'], s['tx'], s['rx']))
      if s['sendBacklog'] > 0:
        self.win.addstr('[backlog {}]'.format(s['sendBacklog']))
    return y
    
  def display_dht(self, y, data):
    y += 2
    self.win.move(y, 1)
    self.win.addstr("DHT:")
    y += 1
    self.win.move(y, 1)
    self.win.addstr("introset lookups")
    y = self.display_bucket(y, data["pendingIntrosetLookups"])
    y += 1
    self.win.move(y, 1)
    self.win.addstr("router lookups")
    return self.display_bucket(y, data["pendingRouterLookups"])

  def display_bucket(self, y, data):
    txs = data['tx']
    self.win.addstr(" ({} lookups)".format(len(txs)))
    for tx in txs:
      y += 1
      self.win.move(y, 1)
      self.win.addstr('search for {}'.format(tx['tx']['target']))
    return y

  def display_data(self):
    """draw main window"""
    if self.data is not None:
      self.win.clear()
      self.win.box()
      self.win.addstr(1,1, "lokinet online")
      #print(self.data)
      services = self.data["services"]
      y = 3
      for k in services:
        y = self.display_service(y, k, services[k])
      y = self.display_links(y, self.data["links"])
      y = self.display_dht(y, self.data["dht"]) 
    self.win.refresh()
    


  def run(self):
    while True:
      self.update_data()
      self.display_data()
      time.sleep(0.5)



if __name__ == '__main__':
  c = Canvas()
  c.run()
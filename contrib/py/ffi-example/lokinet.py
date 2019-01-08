#!/usr/bin/env python3


from ctypes import *
import signal
import time
import threading
import os

lib_file = os.path.join(os.path.realpath('.'), 'liblokinet.so')

class LokiNET(threading.Thread):

    lib = None
    ctx = None

    def load(self, lib, conf):
        self.lib = CDLL(lib)
        self.lib.llarp_ensure_config(conf)
        self.ctx = self.lib.llarp_main_init(conf)
        return self.ctx != 0

    def inform_fail(self):
        """
        inform lokinet crashed
        """

    def inform_end(self):
        """
        inform lokinet ended clean
        """


    def signal(self, sig):
        if self.ctx and self.lib:
            self.lib.llarp_main_signal(self.ctx, int(sig))

    def run(self):
        code = self.lib.llarp_main_run(self.ctx)
        print("llarp_main_run exited with status {}".format(code))
        if code:
            self.inform_fail()
        else:
            self.inform_end()
            
    def close(self):
        if self.lib and self.ctx:
            self.lib.llarp_main_free(self.ctx)

def main():
    loki = LokiNET()
    if loki.load(lib_file, b'daemon.ini'):
        if loki.configure():
            loki.start()
        else:
            print("failed to configure lokinet context")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            llarp.signal(signal.SIGINT)
        finally:
            loki.close()
            return


if __name__ == '__main__':
    main()

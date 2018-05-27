#!/usr/bin/env python3


from ctypes import *
import signal
import time
import threading

class LLARP(threading.Thread):

    lib = None
    ctx = None
    
    def signal(self, sig):
        if self.ctx and self.lib:     
            self.lib.llarp_main_signal(self.ctx, int(sig))
            
    def run(self):
        code = self.lib.llarp_main_run(self.ctx)
        print ("llarp_main_run exited with status {}".format(code))
            

def main():
    llarp = LLARP()
    llarp.lib = CDLL("./libllarp.so")
    llarp.ctx = llarp.lib.llarp_main_init(b'daemon.ini')
    if llarp.ctx:
        llarp.start()
        try:
            while True:
                print("busy loop")
                time.sleep(1)
        except KeyboardInterrupt:
            llarp.signal(signal.SIGINT)
        finally:
            llarp.lib.llarp_main_free(llarp.ctx)
            return

if __name__ == '__main__':
    main()
    

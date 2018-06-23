from _pytuntap import *
import io

class TunTapFileIO(io.FileIO):
    def __init__(self, tuntap):
        super(TunTapFileIO, self).__init__(tuntap.native_handle, 'rb+')

    def read(self, size):
        return memoryview(bytearray(super(TunTapFileIO, self).read(size)))

    def readinto():
        raise NotImplementedError

    def readall():
        raise NotImplementedError

    def writelines():
        raise NotImplementedError

def _file(self):
    return TunTapFileIO(self)

Tap.file = _file
Tun.file = _file

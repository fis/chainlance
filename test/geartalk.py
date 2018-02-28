import subprocess

import geartalk_pb2
from google.protobuf.internal import decoder
from google.protobuf.internal import encoder

_POINT_CHARS = {
    -1: '<',
    0:  'X',
    1:  '>',
}

class Client(object):
    def __init__(self, prog, hill_size):
        self._prog = prog
        self._hill_size = hill_size

    def __enter__(self):
        self._process = subprocess.Popen(
            [self._prog, str(self._hill_size)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._process.communicate()
        if not exc_type and self._process.returncode != 0:
            raise RuntimeError('geartalk program failed with return code {}'.format(self._process.returncode))

    def Set(self, index, code):
        self._Write(geartalk_pb2.Command(action=geartalk_pb2.SET, index=index))
        self._process.stdin.write(code.replace('\0', ' ') + '\0')
        self._process.stdin.flush()
        reply = self._Read(geartalk_pb2.Reply())
        if not reply.ok:
            raise RuntimeError(reply.error or 'geartalk error on set')

    def Test(self, code):
        self._Write(geartalk_pb2.Command(action=geartalk_pb2.TEST))
        self._process.stdin.write(code.replace('\0', ' ') + '\0')
        self._process.stdin.flush()
        reply = self._Read(geartalk_pb2.Reply())
        if not reply.ok:
            raise RuntimeError(reply.error or 'geartalk error on test')

        results = []

        for _ in xrange(self._hill_size):
            # TODO: if reply.with_statistics
            joust = self._Read(geartalk_pb2.Joust())
            results.append('{} {} {}'.format(
                ''.join(_POINT_CHARS[p] for p in joust.sieve_points),
                ''.join(_POINT_CHARS[p] for p in joust.kettle_points),
                -(sum(joust.sieve_points) + sum(joust.kettle_points))))
            pass

        return results

    def _Write(self, message):
        data = message.SerializeToString()
        self._WriteVarint(len(data))
        self._process.stdin.write(data)

    def _Read(self, message):
        data_len = self._ReadVarint()
        data = self._process.stdout.read(data_len)
        message.MergeFromString(data)
        return message

    def _WriteVarint(self, value):
        data = bytearray()
        while True:
            byte = value & 127
            value >>= 7
            if value > 0: byte |= 0x80
            data.append(byte)
            if value == 0: break
        self._process.stdin.write(data)

    def _ReadVarint(self):
        value, bit = 0, 0
        while True:
            byte = self._process.stdout.read(1)
            if not byte: raise RuntimeError('truncated varint from geartalk program')
            byte = ord(byte)
            value |= (byte & 127) << bit
            if not byte & 128: break
            bit += 7
        return value

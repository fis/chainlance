import struct
import subprocess

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
        cmd = struct.pack('<II', 0x02 | (index << 8), len(code))
        self._process.stdin.write(cmd)
        self._process.stdin.write(code)
        self._process.stdin.flush()
        ok, reply = self._Read()
        if not ok:
            raise RuntimeError(reply or 'geartalk error on set')

    def Test(self, code):
        cmd = struct.pack('<II', 0x01, len(code))
        self._process.stdin.write(cmd)
        self._process.stdin.write(code)
        self._process.stdin.flush()
        ok, reply = self._Read()
        if not ok:
            raise RuntimeError(reply or 'geartalk error on test')

        min_tape, max_tape = reply[1], reply[2]
        tapes = max_tape - min_tape + 1

        results = []

        for _ in range(self._hill_size):
            # TODO: statistics flag
            joust = self._process.stdout.read(2*tapes)
            if len(joust) != 2*tapes:
                raise RuntimeError('short read for scores')
            joust = struct.unpack('{}b'.format(2*tapes), joust)
            results.append('{} {} {}'.format(
                ''.join(_POINT_CHARS[p] for p in joust[:tapes]),
                ''.join(_POINT_CHARS[p] for p in joust[tapes:]),
                -sum(joust)))

        return results

    def _Read(self):
        header = self._process.stdout.read(4)
        if len(header) < 4:
            return False, 'short read'
        if (header[0] & 0x01) == 0:
            error = self._process.stdout.read(struct.unpack('<I', header) >> 8)
            return False, error
        return True, header

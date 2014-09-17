import subprocess as sp

class Gear:
    """Minimal Python wrapper for the 'gearlanced' tool."""

    def __init__(self, n, bin='gearlanced'):
        self._hillsize = n
        self._gear = sp.Popen([bin, '{0:d}'.format(n)],
                              stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE)

    def cleanup(self):
        if self._gear.poll() is not None:
            self._gear.kill()
        self._gear.wait()

    def test(self, code):
        self._gear.stdin.write(b'test\n')
        self._gear.stdin.write(code.replace(b'\n', b' ') + b'\n')
        self._gear.stdin.flush()

        result = self._gear.stdout.readline().decode('ascii').rstrip()
        if result != 'ok':
            return result, None

        result = [0] * self._hillsize
        for i in range(self._hillsize):
            points = self._gear.stdout.readline().decode('ascii').rstrip().split(' ')

            if len(points) != 2 or len(points[0]) != len(points[1]):
                raise Exception('gearlanced produced gibberish out: {0}'.format(points))
            if points[0].lstrip('<>X') != '' or points[1].lstrip('<>X') != '':
                raise Exception('gearlanced produced gibberish out: {0}'.format(points))

            pointmap = {'<': -1, 'X': 0, '>': 1}
            result[i] = [pointmap[c] for c in points[0] + points[1]]

        return None, result
 
    def set(self, i, code):
        self._gear.stdin.write(b'set ' + '{0:d}'.format(i).encode('ascii') + b'\n')
        self._gear.stdin.write(code.replace(b'\n', b' ') + b'\n')
        self._gear.stdin.flush()

        result = self._gear.stdout.readline().decode('ascii').rstrip()
        if result == 'ok':
            return None
        else:
            return result

    def unset(self, i):
        self._gear.stdin.write(b'unset ' + '{0:d}'.format(i).encode('ascii') + b'\n')
        self._gear.stdin.flush()

        result = self._gear.stdout.readline().decode('ascii').rstrip()
        if result == 'ok':
            return None
        else:
            return result

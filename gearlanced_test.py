import sys

from test import geartalk
from test import reference
from test import source
from tools import runfiles

def main():
    warriors = source.warriors()
    failed = 0

    with geartalk.Client(runfiles.path('gearlanced'), len(warriors)) as hill:
        for left in warriors:
            with open(left.path, 'rb') as f:
                hill.Set(left.index, f.read())

        for right in warriors:
            with open(right.path, 'rb') as f:
                results = hill.Test(f.read())
            if len(results) != len(warriors):
                raise AssertionError('unexpected number of results: {}: expected {}'.format(
                    len(results), len(warriors)))
            for left, result in zip(warriors, results):
                if not reference.check(left.name, right.name, result):
                    failed += 1

    if failed:
        sys.stderr.write('FAILED TESTS: {}\n'.format(failed))
        sys.exit(1)

    sys.exit(0)

if __name__ == '__main__':
    main()

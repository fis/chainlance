import subprocess
import sys

from test import reference
from test import source
from tools import runfiles

def main():
    failed = 0

    for left, right in source.pairs():
        result = subprocess.check_output(
            [runfiles.path('gearlance'), left.path, right.path])
        if not reference.check(left.name, right.name, result.decode('utf-8')):
            failed += 1

    if failed:
        sys.stderr.write('FAILED TESTS: {}\n'.format(failed))
        sys.exit(1)

    sys.exit(0)

if __name__ == '__main__':
    main()

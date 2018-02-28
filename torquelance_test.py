import itertools
import os.path
import shutil
import subprocess
import sys
import tempfile

from test import reference
from test import source
from tools import runfiles

def main():
    failed = 0

    workdir = tempfile.mkdtemp(prefix='torquelance-test-')
    try:
        for w in source.warriors():
            compiled = os.path.join(workdir, str(w.index))
            subprocess.check_call([runfiles.path('torquelance-compile'), w.path, compiled + '.A', 'A'])
            subprocess.check_call([runfiles.path('torquelance-compile'), w.path, compiled + '.B', 'B'])
            subprocess.check_call([runfiles.path('torquelance-compile'), w.path, compiled + '.B2', 'B2'])

        for left, right in source.pairs():
            result = subprocess.check_output(
                [runfiles.path('torquelance'),
                 os.path.join(workdir, str(left.index) + '.A'),
                 os.path.join(workdir, str(right.index) + '.B'),
                 os.path.join(workdir, str(right.index) + '.B2')])
            if not reference.check(left.name, right.name, result):
                failed += 1
    finally:
        shutil.rmtree(workdir)

    if failed:
        sys.stderr.write('FAILED TESTS: {}\n'.format(failed))
        sys.exit(1)

    sys.exit(0)

if __name__ == '__main__':
    main()

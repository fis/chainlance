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

    workdir = tempfile.mkdtemp(prefix='wrenchlance-test-')
    try:
        for w in source.warriors():
            compiled = os.path.join(workdir, str(w.index))
            subprocess.check_call([runfiles.path('wrenchlance-left'), w.path, compiled + '-left.bin'])
            with open(compiled + '-right.s', 'wb') as f:
                subprocess.check_call([runfiles.path('wrenchlance-right'), w.path], stdout=f)
            subprocess.check_call(
                ['gcc', '-std=gnu11', '-fwhole-program', '-O2', '-march=native',
                 '-I' + os.path.dirname(runfiles.path('common.h')),
                 runfiles.path('wrenchlance-stub.c'),
                 runfiles.path('wrenchlance-header.s'),
                 compiled + '-right.s',
                 '-o', compiled + '-right'])

        for left, right in source.pairs():
            result = subprocess.check_output(
                [os.path.join(workdir, str(right.index) + '-right'),
                 os.path.join(workdir, str(left.index) + '-left.bin')])
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

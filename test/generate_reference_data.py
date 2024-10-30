import subprocess
import sys

from test import source
from tools import runfiles

def main():
    for left, right in source.pairs():
        result = subprocess.check_output(
            ['nodejs', runfiles.path('test/egojsout.js'), left.path, right.path])
        sys.stdout.write(result.decode('utf-8'))

if __name__ == '__main__':
    main()

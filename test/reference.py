import sys

from test import source
from tools import runfiles

_results = None

def check(left, right, result):
    result = result.rstrip('\n')

    global _results

    if _results is None:
        pairs = list(source.pairs())
        with runfiles.open('test/reference.data') as f:
            data = [line.rstrip('\n') for line in f.readlines()]

        if len(pairs) != len(data):
            raise AssertionError(
                'reference data mismatch: {} pairs, {} results'.format(len(pairs), len(data)))

        _results = dict(
            ((p[0].name, p[1].name), r)
            for p, r in zip(pairs, data)
        )

    ref = _results.get((left, right))

    if not ref:
        raise AssertionError('reference data missing: ({}, {})'.format(left, right))
    if ref != result:
        sys.stderr.write('FAILED: {} {}\n'.format(left, right))
        sys.stderr.write('EXPECTED: ' + ref + '\n')
        sys.stderr.write('GOT: ' + result + '\n')
        return False

    return True

import __builtin__
import os
import os.path
import re
import sys

_WORKSPACE = 'fi_zem_chainlance'

_runfiles_dir = None

def _find_runfiles():
    stub = os.path.abspath(sys.argv[0])

    runfiles = stub + '.runfiles'
    if os.path.isdir(runfiles):
        return runfiles

    m = re.match(r'(.*\.runfiles)/', stub)
    if m:
        return m.group(1)

    raise AssertionError('unable to find runfiles for: ' + stub)

def _runfiles():
    global _runfiles_dir
    if _runfiles_dir is None:
        _runfiles_dir = _find_runfiles()
    return _runfiles_dir

def path(path):
    return os.path.join(_runfiles(), _WORKSPACE, path)

def open(file, *args, **kwargs):
    return __builtin__.open(path(file), *args, **kwargs)

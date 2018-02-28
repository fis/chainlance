import collections

from tools import runfiles

_Warrior = collections.namedtuple('_Warrior', ['index', 'name', 'path'])

_warrior_list = None

def _warriors():
    global _warrior_list
    if _warrior_list is None:
        with runfiles.open('test/warriors.idx') as f:
            _warrior_list = [
                _Warrior(
                    index = i,
                    name = w.rstrip('\n'),
                    path = runfiles.path('test/' + w.rstrip('\n'))
                ) for i, w in enumerate(f.readlines())
            ]
    return _warrior_list

def count():
    return len(_warriors())

def warriors():
    return list(_warriors())

def pairs():
    warriors = _warriors()
    for left in warriors:
        for right in warriors:
            yield left, right
